#pragma once

#include "ui/models/PageModel.h"

class PreviewOnboardingModel final : public PageModel {
    Q_OBJECT
public:
    explicit PreviewOnboardingModel(QObject* parent = nullptr);
    void show();
    Q_INVOKABLE void begin();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void act() { next(); }
    Q_INVOKABLE void secondaryAction() { skip(); }

signals:
    void navigateRequested(const QString& page);

private:
    void refresh();
    int step_ = -1;
    bool active_ = false;
};
