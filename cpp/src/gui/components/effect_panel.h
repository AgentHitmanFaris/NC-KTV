#pragma once
/*
 * NC-KTV GUI — Effect Panel
 * Add/edit/remove effects per clip with type selection and parameter controls
 */

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLabel>
#include "timeline/timeline_data.h"

namespace ncktv {

class CurveEditor;

class EffectPanel : public QWidget {
    Q_OBJECT

public:
    explicit EffectPanel(QWidget* parent = nullptr);

    void setClip(Clip* clip);

signals:
    void effectsChanged();

private slots:
    void onAddEffect();
    void onRemoveEffect();
    void onSelectionChanged();
    void onParameterChanged();

private:
    void setupUi();
    void applyTheme();
    void refreshList();
    void showEffectProperties(const Effect* effect);

    Clip* m_clip = nullptr;

    QListWidget*     m_effectList = nullptr;
    QComboBox*       m_typeCombo = nullptr;
    QDoubleSpinBox*  m_startSpin = nullptr;
    QDoubleSpinBox*  m_durationSpin = nullptr;
    QComboBox*       m_easingCombo = nullptr;
    CurveEditor*     m_curveEditor = nullptr;
    QPushButton*     m_addBtn = nullptr;
    QPushButton*     m_removeBtn = nullptr;
};

} // namespace ncktv
