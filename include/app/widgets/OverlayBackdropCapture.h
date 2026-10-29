#pragma once

#include <QImage>
#include <QRect>
#include <memory>

class OverlayBackdropCapture {
public:
    OverlayBackdropCapture();
    ~OverlayBackdropCapture();
    QImage grab(const QRect& physicalRect);
    void reset();

private:
    struct State;
    std::unique_ptr<State> state_;
};
