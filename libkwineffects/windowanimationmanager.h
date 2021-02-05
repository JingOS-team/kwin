/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef WINDOWANIMATIONMANAGER_H
#define WINDOWANIMATIONMANAGER_H

#include <QObject>
#include "kwineffects.h"
#include <kwineffects_export.h>
namespace KWin {

class KWINEFFECTS_EXPORT  Animation : public QObject
{
    Q_OBJECT
public:
    explicit Animation(QObject *parent = nullptr);
    virtual ~Animation();

    void initAnimate(QEasingCurve::Type type, std::chrono::milliseconds duration = std::chrono::milliseconds(1000),
              std::chrono::milliseconds delay = std::chrono::milliseconds::zero(), TimeLine::Direction direction = TimeLine::Forward);

    virtual void updateTime(std::chrono::milliseconds presentTime);

    virtual void updateValue() = 0;
    TimeLine timeLine() {
        return _timeLine;
    }

Q_SIGNALS:
    void finished();

protected:
    bool _isDelay = false;
    TimeLine _timeLine;
    std::chrono::milliseconds _duration = std::chrono::milliseconds::zero();
    std::chrono::milliseconds _lastPresentTime = std::chrono::milliseconds::zero();
};

class KWINEFFECTS_EXPORT Animation1F : public Animation
{
    Q_OBJECT
public:
    explicit Animation1F(QObject *parent = nullptr);

    void initValue(const qreal &start, const qreal &target);
    void updateValue() override;

    qreal getValue() {
        return _value;
    }
    qreal getStart() {
        return _start;
    }
    qreal getTarget() {
        return _target;
    }

Q_SIGNALS:
    void valueChanged(qreal value);

protected:
    qreal _value;
    qreal _start;
    qreal _target;
};


class KWINEFFECTS_EXPORT BoundAnimation1F : public Animation1F
{
    Q_OBJECT
public:
    explicit BoundAnimation1F(QObject *parent = nullptr);
    void updateValue() override;

private:
    bool _hasRevert = false;
};

class KWINEFFECTS_EXPORT Animation2F : public Animation
{
    Q_OBJECT
public:
    explicit Animation2F(QObject *parent = nullptr);

    void initValue(const QPointF &start, const QPointF &target);
    void updateValue() override;

    QPointF getValue() {
        return _value;
    }
    QPointF getStart() {
        return _start;
    }
    QPointF getTarget() {
        return _target;
    }

Q_SIGNALS:
    void valueChanged(const QPointF &value);

protected:
    QPointF _value;
    QPointF _start;
    QPointF _target;
};

class KWINEFFECTS_EXPORT  BoundAnimation2F : public Animation2F
{
    Q_OBJECT
public:
    explicit BoundAnimation2F(QObject *parent = nullptr);
    void updateValue() override;

private:
    bool _hasRevert = false;
};

class KWINEFFECTS_EXPORT Motions : public QObject
{
    Q_OBJECT
public:
    explicit Motions(QObject *parent = nullptr);
    virtual ~Motions();

    virtual void updateTime(std::chrono::milliseconds presentTime) = 0;

    void addTranslateAnimate(Animation2F* animation);
    void addScaleAnimate(Animation2F* animation);
    void addOpacityAnimate(Animation1F* animation);

    QPointF getCurPos() {
        return _curPos;
    }
    QPointF getCurScale() {
        return _curScale;
    }
    qreal getCurOpacity() {
        return _curOpacity;
    }

Q_SIGNALS:
    void finished();

private:
    void addAnimateFinished(Animation *animation);

protected:
    QList<Animation*> animations;

private:
    QPointF _curPos = QPointF(0, 0);
    QPointF _curScale = QPointF(1., 1.);
    qreal _curOpacity = 1.;
};

class KWINEFFECTS_EXPORT ParallelMotions : public Motions
{
    Q_OBJECT
public:
    explicit ParallelMotions(QObject *parent = nullptr);

    void updateTime(std::chrono::milliseconds presentTime) override;

};

class KWINEFFECTS_EXPORT  SequentialMotions: public Motions
{
    Q_OBJECT
public:
    explicit SequentialMotions(QObject *parent = nullptr);

    void updateTime(std::chrono::milliseconds presentTime) override;

};

class KWINEFFECTS_EXPORT WindowAnimationManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowAnimationManager(QObject *parent = nullptr);
    ~WindowAnimationManager();

    WindowAnimationManager(const WindowAnimationManager& other);

    WindowAnimationManager& operator =(const WindowAnimationManager& other);

    void manage(KWin::EffectWindow* w, Motions *motion);
    void unmanage(KWin::EffectWindow* w);

    bool isManaging(KWin::EffectWindow* w) {
        return _windowAnimations.contains(w);
    }

    bool isEmpty() const {
        return _windowAnimations.isEmpty() && _windowFinished.isEmpty();
    }

    bool hasFinished() {
        for (auto it = _windowAnimations.keys().begin(); it != _windowAnimations.keys().end(); it++) {
            if (!_windowFinished.contains(*it)) {
                return false;
            }
        }
        return  true;
    }

    Motions* motions(EffectWindow *w);

    void updateTime(std::chrono::milliseconds presentTime);

    bool isFinished(KWin::EffectWindow* w) {
        return _windowFinished.contains(w);
    }

    void unmanageAll() {
        foreach (Motions *motions , _windowAnimations.values()) {
            delete motions;
        }
        _windowAnimations.clear();
        _windowFinished.clear();
    }
    inline EffectWindowList managedWindows() const {
        EffectWindowList result =_windowAnimations.keys();
        result.append(_windowFinished);

        return result;
    }
private:
    QHash<KWin::EffectWindow*, Motions*> _windowAnimations;
    QList<KWin::EffectWindow*> _windowFinished;
};

}
#endif // WINDOWANIMATIONMANAGER_H
