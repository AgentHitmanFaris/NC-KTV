#pragma once
#include <QObject>
namespace ncktv {
class TimingOffsetHandler : public QObject {
    Q_OBJECT

public: explicit TimingOffsetHandler(QObject* parent = nullptr); };
} // namespace ncktv
