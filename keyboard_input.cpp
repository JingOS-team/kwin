/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_input.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "keyboard_layout.h"
#include "keyboard_repeat.h"
#include "abstract_client.h"
#include "effects.h"
#include "modifier_only_shortcuts.h"
#include "utils.h"
#include "screenlockerwatcher.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
#include "platform.h"

// KWayland
#include <KWaylandServer/datadevice_interface.h>
#include <KWaylandServer/seat_interface.h>
//screenlocker
#include <KScreenLocker/KsldApp>
// Frameworks
#include <KGlobalAccel>
// Qt
#include <QKeyEvent>

#include <linux/input-event-codes.h>

namespace KWin
{

KeyboardInputRedirection::KeyboardInputRedirection(InputRedirection *parent)
    : QObject(parent)
    , m_input(parent)
    , m_xkb(new Xkb(parent))
{
    connect(m_xkb.data(), &Xkb::ledsChanged, this, &KeyboardInputRedirection::ledsChanged);
    connect(m_xkb.data(), &Xkb::capsChanged, this, &KeyboardInputRedirection::capsChanged);
    if (waylandServer()) {
        m_xkb->setSeat(waylandServer()->seat());
    }
}

KeyboardInputRedirection::~KeyboardInputRedirection() = default;

class KeyStateChangedSpy : public InputEventSpy
{
public:
    KeyStateChangedSpy(InputRedirection *input)
        : m_input(input)
    {
    }

    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }
        emit m_input->keyStateChanged(event->nativeScanCode(), event->type() == QEvent::KeyPress ? InputRedirection::KeyboardKeyPressed : InputRedirection::KeyboardKeyReleased);
    }

private:
    InputRedirection *m_input;
};

class ModifiersChangedSpy : public InputEventSpy
{
public:
    ModifiersChangedSpy(InputRedirection *input)
        : m_input(input)
        , m_modifiers()
    {
    }

    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }
        updateModifiers(event->modifiers());
    }

    void updateModifiers(Qt::KeyboardModifiers mods)
    {
        if (mods == m_modifiers) {
            return;
        }
        emit m_input->keyboardModifiersChanged(mods, m_modifiers);
        m_modifiers = mods;
    }

private:
    InputRedirection *m_input;
    Qt::KeyboardModifiers m_modifiers;
};

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    const auto config = kwinApp()->kxkbConfig();
    m_xkb->setNumLockConfig(InputConfig::self()->inputConfig());
    m_xkb->setConfig(config);

    m_input->installInputEventSpy(new KeyStateChangedSpy(m_input));
    m_modifiersChangedSpy = new ModifiersChangedSpy(m_input);
    m_input->installInputEventSpy(m_modifiersChangedSpy);
    m_keyboardLayout = new KeyboardLayout(m_xkb.data(), config);
    m_keyboardLayout->init();
    m_input->installInputEventSpy(m_keyboardLayout);

    if (waylandServer()->hasGlobalShortcutSupport()) {
        m_input->installInputEventSpy(new ModifierOnlyShortcuts);
    }

    KeyboardRepeat *keyRepeatSpy = new KeyboardRepeat(m_xkb.data());
    connect(keyRepeatSpy, &KeyboardRepeat::keyRepeat, this,
        std::bind(&KeyboardInputRedirection::processKey, this, std::placeholders::_1, InputRedirection::KeyboardKeyAutoRepeat, std::placeholders::_2, nullptr));
    m_input->installInputEventSpy(keyRepeatSpy);

    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(workspace(), &Workspace::clientActivated, this,
        [this] {
            disconnect(m_activeClientSurfaceChangedConnection);
            if (auto c = workspace()->activeClient()) {
                m_activeClientSurfaceChangedConnection = connect(c, &Toplevel::surfaceChanged, this, &KeyboardInputRedirection::update);
            } else {
                m_activeClientSurfaceChangedConnection = QMetaObject::Connection();
            }
            update();
        }
    );
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &KeyboardInputRedirection::update);
    }
}

void KeyboardInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    auto seat = waylandServer()->seat();
    // TODO: this needs better integration
    Toplevel *found = nullptr;
    if (waylandServer()->isScreenLocked()) {
        const QList<Toplevel *> &stacking = Workspace::self()->stackingOrder();
        if (!stacking.isEmpty()) {
            auto it = stacking.end();
            do {
                --it;
                Toplevel *t = (*it);
                if (t->isDeleted()) {
                    // a deleted window doesn't get mouse events
                    continue;
                }
                if (!t->isLockScreen()) {
                    continue;
                }
                if (!t->readyForPainting()) {
                    continue;
                }
                found = t;
                break;
            } while (it != stacking.begin());
        }
    } else if (!input()->isSelectingWindow()) {
        found = workspace()->activeClient();
    }
    if (found && found->surface()) {
        if (found->surface() != seat->focusedKeyboardSurface()) {
            seat->setFocusedKeyboardSurface(found->surface());
            m_at = found;
        }
    } else {
        seat->setFocusedKeyboardSurface(nullptr);
        m_at = nullptr;
    }
}


bool KeyboardInputRedirection::preProcessKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time, LibInput::Device *device)
{
    Q_UNUSED(time);
    Q_UNUSED(device);
    if (state == InputRedirection::KeyboardKeyReleased) {
        if (key == 172) { // HOME_PAGE
            effects->closeTask();
            Workspace::self()->setShowingDesktop(true, false, true);
            return true;
        } else if (key == 580) { // SELECT_TASK
            effects->toTriggerTask();
        }
    }
    if (key == 172) {
        return true;
    }
    return false;
}

Toplevel *KeyboardInputRedirection::at() const
{
    return m_at;
}

void KeyboardInputRedirection::processKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time, LibInput::Device *device)
{
    if (key == 58) {
        qDebug()<<"CAPS_DEBUG processKey 58:"<<state;
    }
    qDebug()<<"KEY_DEBUG:"<<Q_FUNC_INFO<<__LINE__<<" key:"<<key<<" state:"<<state;
    if (!kwinApp()->platform()->isSetupMode() && preProcessKey(key, state, time, device)) {
        return;
    }

    QEvent::Type type;
    bool autoRepeat = false;
    switch (state) {
    case InputRedirection::KeyboardKeyAutoRepeat:
        autoRepeat = true;
        // fall through
    case InputRedirection::KeyboardKeyPressed:
        type = QEvent::KeyPress;
        break;
    case InputRedirection::KeyboardKeyReleased:
        type = QEvent::KeyRelease;
        break;
    default:
        Q_UNREACHABLE();
    }

    const quint32 previousLayout = m_xkb->currentLayout();
    if (!autoRepeat) {
        m_xkb->updateKey(key, state);
    }

    const xkb_keysym_t keySym = m_xkb->currentKeysym();
    Qt::Key qtKey = m_xkb->toQtKey(keySym);
    KeyEvent event(type,
                   qtKey,
                   m_xkb->modifiers(),
                   key,
                   keySym,
                   m_xkb->toString(keySym),
                   autoRepeat,
                   time,
                   device);
    event.setModifiersRelevantForGlobalShortcuts(m_xkb->modifiersRelevantForGlobalShortcuts());

    m_input->processSpies(std::bind(&InputEventSpy::keyEvent, std::placeholders::_1, &event));
    if (!m_inited) {
        return;
    }
    m_input->processFilters(std::bind(&InputEventFilter::keyEvent, std::placeholders::_1, &event));

    m_xkb->forwardModifiers();

    if (event.modifiersRelevantForGlobalShortcuts() == Qt::KeyboardModifier::NoModifier && type != QEvent::KeyRelease) {
        m_keyboardLayout->checkLayoutChange(previousLayout);
    }
}

void KeyboardInputRedirection::sendBackKey(uint32_t time) {
    uint32_t key = XKB_KEY_XF86Back;
    m_xkb->updateKey(key, InputRedirection::KeyboardKeyPressed);

    QEvent::Type type =  QEvent::KeyPress;
    const xkb_keysym_t keySym = m_xkb->currentKeysym();
    KeyEvent event(type,
                   m_xkb->toQtKey(keySym),
                   m_xkb->modifiers(),
                   key,
                   keySym,
                   m_xkb->toString(keySym),
                   false,
                   time,
                   nullptr);

    m_input->processFilters(std::bind(&InputEventFilter::keyEvent, std::placeholders::_1, &event));

    m_xkb->updateKey(key, InputRedirection::KeyboardKeyReleased);

    type =  QEvent::KeyRelease;
    KeyEvent event_release(type,
                   m_xkb->toQtKey(keySym),
                   m_xkb->modifiers(),
                   key,
                   keySym,
                   m_xkb->toString(keySym),
                   false,
                   time+10,
                   nullptr);

    m_input->processFilters(std::bind(&InputEventFilter::keyEvent, std::placeholders::_1, &event_release));
}

void KeyboardInputRedirection::sendFakeKey(uint32_t keySym, InputRedirection::KeyboardKeyState state, uint32_t time)
{
    uint32_t key_code = KEY_UNKNOWN;

    switch(keySym) {
    case XKB_KEY_BackSpace:
        key_code = KEY_BACKSPACE;
        break;
    case XKB_KEY_Return:
        key_code = KEY_ENTER;
        break;
    case XKB_KEY_space:
        key_code = KEY_SPACE;
        break;
    case XKB_KEY_Left:
        key_code = KEY_LEFT;
        break;
    case XKB_KEY_Right:
        key_code = KEY_RIGHT;
        break;
    case XKB_KEY_Up:
        key_code = KEY_UP;
        break;
    case XKB_KEY_Down:
        key_code = KEY_DOWN;
        break;
    case XKB_KEY_Shift_L:
        key_code = KEY_LEFTSHIFT;
        break;
    case XKB_KEY_Shift_R:
        key_code = KEY_RIGHTSHIFT;
        break;
    case XKB_KEY_Control_L:
        key_code = KEY_LEFTCTRL;
        break;
    case XKB_KEY_Control_R:
        key_code = KEY_RIGHTCTRL;
        break;
    case XKB_KEY_Alt_L:
        key_code = KEY_LEFTALT;
        break;
    case XKB_KEY_Alt_R:
        key_code = KEY_RIGHTALT;
        break;
    case XKB_KEY_Insert:
        key_code = KEY_INSERT;
        break;
    case XKB_KEY_Tab:
        key_code = KEY_TAB;
        break;
    case XKB_KEY_Escape:
        key_code = KEY_ESC;
        break;
    default:
        key_code = KEY_UNKNOWN;
        break;
    }

    if (KEY_UNKNOWN == key_code)
        return;

    processKey(key_code, state, time);
}

bool KeyboardInputRedirection::isCapsOn()
{
    return m_xkb->isCapsOn();
}

void KeyboardInputRedirection::processModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_inited) {
        return;
    }
    const quint32 previousLayout = m_xkb->currentLayout();
    // TODO: send to proper Client and also send when active Client changes
    m_xkb->updateModifiers(modsDepressed, modsLatched, modsLocked, group);
    m_modifiersChangedSpy->updateModifiers(modifiers());
    m_keyboardLayout->checkLayoutChange(previousLayout);
}

void KeyboardInputRedirection::processKeymapChange(int fd, uint32_t size)
{
    if (!m_inited) {
        return;
    }
    // TODO: should we pass the keymap to our Clients? Or only to the currently active one and update
    m_xkb->installKeymap(fd, size);
    m_keyboardLayout->resetLayout();
}

}
