/*
 * NC-KTV GUI — Effect Panel Implementation
 */

#include "effect_panel.h"
#include "curve_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

namespace ncktv {

EffectPanel::EffectPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
    applyTheme();
}

void EffectPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    auto* titleLabel = new QLabel("✨ Effects", this);
    titleLabel->setStyleSheet("font-size:14px; font-weight:bold; color:#ccc; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Effect List ──────────────────────────────────────────────────────
    m_effectList = new QListWidget(this);
    m_effectList->setMaximumHeight(120);
    mainLayout->addWidget(m_effectList);

    auto* listBtnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("+ Add", this);
    m_removeBtn = new QPushButton("✕ Remove", this);
    listBtnLayout->addWidget(m_addBtn);
    listBtnLayout->addWidget(m_removeBtn);
    listBtnLayout->addStretch();
    mainLayout->addLayout(listBtnLayout);

    // ── Properties ───────────────────────────────────────────────────────
    auto* propsGroup = new QGroupBox("Properties", this);
    auto* propsForm = new QFormLayout(propsGroup);
    propsForm->setSpacing(8);
    propsForm->setContentsMargins(12, 16, 12, 10);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({
        "Fade In", "Fade Out", "Slide Left", "Slide Right",
        "Zoom In", "Zoom Out", "Blur", "Color Shift", "Custom"
    });
    propsForm->addRow("Type:", m_typeCombo);

    m_startSpin = new QDoubleSpinBox(this);
    m_startSpin->setRange(0, 99999);
    m_startSpin->setDecimals(3);
    m_startSpin->setSuffix(" s");
    propsForm->addRow("Start:", m_startSpin);

    m_durationSpin = new QDoubleSpinBox(this);
    m_durationSpin->setRange(0.01, 99999);
    m_durationSpin->setDecimals(3);
    m_durationSpin->setValue(1.0);
    m_durationSpin->setSuffix(" s");
    propsForm->addRow("Duration:", m_durationSpin);

    m_easingCombo = new QComboBox(this);
    m_easingCombo->addItems({"Linear", "Ease In", "Ease Out", "Ease In Out", "Custom Bézier"});
    propsForm->addRow("Easing:", m_easingCombo);

    mainLayout->addWidget(propsGroup);

    // ── Curve Editor ─────────────────────────────────────────────────────
    m_curveEditor = new CurveEditor(this);
    m_curveEditor->setFixedHeight(150);
    mainLayout->addWidget(m_curveEditor);

    mainLayout->addStretch();

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_addBtn, &QPushButton::clicked, this, &EffectPanel::onAddEffect);
    connect(m_removeBtn, &QPushButton::clicked, this, &EffectPanel::onRemoveEffect);
    connect(m_effectList, &QListWidget::currentRowChanged, this, [this](int) { onSelectionChanged(); });
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPanel::onParameterChanged);
    connect(m_startSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EffectPanel::onParameterChanged);
    connect(m_durationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EffectPanel::onParameterChanged);
    connect(m_easingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_curveEditor->setCurve(static_cast<EasingCurve>(idx));
        onParameterChanged();
    });
}

void EffectPanel::applyTheme() {
    setStyleSheet(R"(
        QWidget { background:#1e1e1e; color:#d4d4d4; }
        QListWidget { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; color:#d4d4d4; }
        QListWidget::item { padding:6px; border-bottom:1px solid #3f3f46; }
        QListWidget::item:selected { background:#4f46e5; color:white; }
        QGroupBox { border:1px solid #3f3f46; border-radius:4px; padding-top:14px; margin-top:6px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }
        QComboBox, QDoubleSpinBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:4px 6px; color:#fff; }
        QComboBox::drop-down { border:none; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:5px 10px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QLabel { border:none; }
    )");
}

void EffectPanel::setClip(Clip* clip) {
    m_clip = clip;
    refreshList();
}

void EffectPanel::refreshList() {
    m_effectList->clear();
    if (!m_clip) return;

    for (int i = 0; i < m_clip->effects.size(); ++i) {
        const auto& e = m_clip->effects[i];
        QString label = effectTypeToString(e.effectType) +
                       QString(" (%.2fs, %1s)").arg(e.startTime, 0, 'f', 2).arg(e.duration, 0, 'f', 2);
        m_effectList->addItem(label);
    }
}

void EffectPanel::showEffectProperties(const Effect* effect) {
    if (!effect) return;
    m_typeCombo->blockSignals(true);
    m_startSpin->blockSignals(true);
    m_durationSpin->blockSignals(true);
    m_easingCombo->blockSignals(true);

    m_typeCombo->setCurrentIndex(static_cast<int>(effect->effectType));
    m_startSpin->setValue(effect->startTime);
    m_durationSpin->setValue(effect->duration);
    m_easingCombo->setCurrentIndex(static_cast<int>(effect->easing));
    m_curveEditor->setCurve(effect->easing);
    if (!effect->customCurvePoints.isEmpty()) {
        m_curveEditor->setCustomPoints(effect->customCurvePoints);
    }

    m_typeCombo->blockSignals(false);
    m_startSpin->blockSignals(false);
    m_durationSpin->blockSignals(false);
    m_easingCombo->blockSignals(false);
}

void EffectPanel::onAddEffect() {
    if (!m_clip) return;
    Effect e;
    e.effectType = static_cast<EffectType>(m_typeCombo->currentIndex());
    e.startTime = m_startSpin->value();
    e.duration = m_durationSpin->value();
    e.easing = static_cast<EasingCurve>(m_easingCombo->currentIndex());
    m_clip->addEffect(e);
    refreshList();
    emit effectsChanged();
}

void EffectPanel::onRemoveEffect() {
    if (!m_clip) return;
    int row = m_effectList->currentRow();
    if (row >= 0) {
        m_clip->removeEffect(row);
        refreshList();
        emit effectsChanged();
    }
}

void EffectPanel::onSelectionChanged() {
    if (!m_clip) return;
    int row = m_effectList->currentRow();
    if (row >= 0 && row < m_clip->effects.size()) {
        showEffectProperties(&m_clip->effects[row]);
    }
}

void EffectPanel::onParameterChanged() {
    if (!m_clip) return;
    int row = m_effectList->currentRow();
    if (row < 0 || row >= m_clip->effects.size()) return;

    auto& e = m_clip->effects[row];
    e.effectType = static_cast<EffectType>(m_typeCombo->currentIndex());
    e.startTime = m_startSpin->value();
    e.duration = m_durationSpin->value();
    e.easing = static_cast<EasingCurve>(m_easingCombo->currentIndex());
    if (e.easing == EasingCurve::CustomBezier) {
        e.customCurvePoints = m_curveEditor->customPoints();
    }
    refreshList();
    m_effectList->setCurrentRow(row);
    emit effectsChanged();
}

} // namespace ncktv
