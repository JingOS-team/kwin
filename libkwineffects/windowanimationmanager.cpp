/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "windowanimationmanager.h"
namespace KWin {
Animation::Animation(QObject *parent)
{

}

Animation::~Animation()
{

}

void Animation::initAnimate(QEasingCurve::Type type, std::chrono::milliseconds duration, std::chrono::milliseconds delay, TimeLine::Direction direction)
{
    if (delay  > std::chrono::milliseconds::zero()) {
        _isDelay = true;
        _timeLine.setDuration(delay);
        _duration = duration;
    } else {
        _timeLine.setDuration(duration);
    }
    _timeLine.setEasingCurve(type);
    _timeLine.setDirection(direction);
}

void Animation::updateTime(std::chrono::milliseconds presentTime)
{
    std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
    if (_lastPresentTime.count()) {
        delta = presentTime - _lastPresentTime;
    }
    _lastPresentTime = presentTime;

    _timeLine.update(delta);

    if (!_isDelay) {
        updateValue();
    }
    if (_timeLine.done()) {
        if (_isDelay) {
            _isDelay = false;
            _timeLine.reset();
            _timeLine.setDuration(_duration);
            _lastPresentTime = std::chrono::milliseconds::zero();
        } else {
            emit finished();
        }
    }
}

ParallelMotions::ParallelMotions(QObject *parent)
    : Motions(parent)
{

}

void ParallelMotions::updateTime(std::chrono::milliseconds presentTime)
{
    foreach(Animation* animation , animations) {
        animation->updateTime(presentTime);
    }
}

SequentialMotions::SequentialMotions(QObject *parent)
    : Motions(parent)
{

}

void SequentialMotions::updateTime(std::chrono::milliseconds presentTime)
{
    animations.front()->updateTime(presentTime);
}


WindowAnimationManager::WindowAnimationManager(QObject *parent)
    : QObject(parent)
{

}

WindowAnimationManager::~WindowAnimationManager()
{

}

WindowAnimationManager::WindowAnimationManager(const WindowAnimationManager &other)
{
    _windowAnimations = other._windowAnimations;
    _windowFinished = other._windowFinished;
    for (auto it = _windowAnimations.begin(); it != _windowAnimations.end(); it++) {
        EffectWindow *w = it.key();
        connect(it.value(), &Motions::finished, this, [this, w] {
            _windowFinished.append(w);
        });
    }
}

WindowAnimationManager &WindowAnimationManager::operator =(const WindowAnimationManager &other)
{
    _windowAnimations = other._windowAnimations;
    _windowFinished = other._windowFinished;
    for (auto it = _windowAnimations.begin(); it != _windowAnimations.end(); it++) {
        EffectWindow *w = it.key();
        connect(it.value(), &Motions::finished, this, [this, w] {
            _windowFinished.append(w);
        });
    }
    return *this;
}

void WindowAnimationManager::manage(EffectWindow *w, Motions *motion)
{
    if (isManaging(w)) {
        unmanage(w);
    }
    _windowAnimations.insert(w, motion);
    connect(motion, &Motions::finished, this, [this, w] {
        _windowFinished.append(w);
    });
}

void WindowAnimationManager::unmanage(EffectWindow *w)
{
    if (_windowAnimations.contains(w)) {
        delete _windowAnimations[w];
        _windowAnimations.remove(w);
    }
    _windowFinished.removeOne(w);
}

Motions* WindowAnimationManager::motions(EffectWindow *w)
{
    return _windowAnimations.value(w);
}

void WindowAnimationManager::updateTime(std::chrono::milliseconds presentTime)
{
    foreach(Motions* animation, _windowAnimations) {
        animation->updateTime(presentTime);
    }
}

Animation1F::Animation1F(QObject *parent)
    : Animation(parent)
{

}

void Animation1F::initValue(const qreal &start, const qreal &target)
{
    _start = start;
    _target = target;
}

void Animation1F::updateValue()
{
    const qreal progress = _timeLine.value();
    _value = _start + (_target - _start) * progress;
    emit valueChanged(_value);
}

Animation2F::Animation2F(QObject *parent)
    : Animation(parent)
{

}

void Animation2F::initValue(const QPointF &start, const QPointF &target)
{
    _start = start;
    _target = target;
}

void Animation2F::updateValue()
{
    const qreal progress = _timeLine.value();
    _value = _start + (_target - _start) * progress;
    emit valueChanged(_value);
}

BoundAnimation2F::BoundAnimation2F(QObject *parent)
{

}

void BoundAnimation2F::updateValue()
{
    const qreal progress = _timeLine.value();
    if (!_hasRevert && progress >= 0.5) {
        _timeLine.toggleDirection();
        _hasRevert = true;
    }
    _value = _start + (_target - _start) * progress * 2;
    emit valueChanged(_value);
}

Motions::Motions(QObject *parent)
    : QObject(parent)
{

}

Motions::~Motions()
{
    while(!animations.isEmpty()) {
        delete animations.front();
        animations.pop_front();
    }
}

void Motions::addTranslateAnimate(Animation2F *animation)
{
    addAnimateFinished(animation);
    connect(animation, &Animation2F::valueChanged, this, [this](const QPointF &value) {
        _curPos = value;
    });
}

void Motions::addScaleAnimate(Animation2F *animation)
{
    addAnimateFinished(animation);
    connect(animation, &Animation2F::valueChanged, this, [this](const QPointF &value) {
        _curScale = value;
    });
}

void Motions::addOpacityAnimate(Animation1F *animation)
{
    addAnimateFinished(animation);
    connect(animation, &Animation1F::valueChanged, this, [this](qreal value) {
        _curOpacity = value;
    });
}

void Motions::addAnimateFinished(Animation *animation)
{
    animations.append(animation);
    connect(animation, &Animation::finished, this, [this, animation]() {
        animations.removeOne(animation);
        delete animation;
        if (animations.isEmpty()) {
            emit finished();
        }
    });
}

BoundAnimation1F::BoundAnimation1F(QObject *parent)
    : Animation1F(parent)
{

}

void BoundAnimation1F::updateValue()
{
    const qreal progress = _timeLine.value();
    if (!_hasRevert && progress >= 0.5) {
        _timeLine.toggleDirection();
        _hasRevert = true;
    }
    _value = _start + (_target - _start) * progress * 2;
    emit valueChanged(_value);
}

}
