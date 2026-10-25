import React from 'react';
import type {
  AssistedPlacementMode,
  ExperimentalSettingsControls,
} from '../application/useExperimentalSettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloCheckbox, LHoloTextField } from '../components/LHoloControls';

export function ExperimentalPage({ labels, experimental }: {
  labels: MenuLabels; experimental: ExperimentalSettingsControls;
}) {
  const infoButtonLabel = experimental.experimentalConsent
    ? labels['button.experimentalInfoView']
    : labels['button.experimentalInfoOpen'];

  const modeKeyById: Record<AssistedPlacementMode, string> = {
    manual: 'mode.manual',
    easy: 'mode.easy',
    range: 'mode.range',
  };

  const radiusLabel = labels['label.placementRadius'];
  const cooldownLabel = labels['label.autoPlacementCooldown'];

  return <>
    <div className="lholo-section-title">{labels['section.assistedPlacement']}</div>
    <div className="lholo-action-row">
      <LHoloButton label={infoButtonLabel} className="lholo-danger-button"
        onClick={experimental.openInfoModal} />
    </div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!experimental.canCommand} label={labels['checkbox.manualPlace']}
        value={experimental.activeMode === 'manual'}
        onChange={checked => experimental.toggleMode('manual', checked)} />
    </div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!experimental.canCommand} label={labels['checkbox.easyPlace']}
        value={experimental.activeMode === 'easy'}
        onChange={checked => experimental.toggleMode('easy', checked)} />
    </div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!experimental.canCommand} label={labels['checkbox.rangePlace']}
        value={experimental.activeMode === 'range'}
        onChange={checked => experimental.toggleMode('range', checked)} />
    </div>

    {experimental.activeMode ? (
      <div className="lholo-status">
        {labels['hint.modeEnabled'].replace('%s', labels[modeKeyById[experimental.activeMode]])}
      </div>
    ) : !experimental.experimentalConsent ? (
      <div className="lholo-muted">{labels['hint.assistedDisabledByConsent']}</div>
    ) : (
      <div className="lholo-muted">{labels['hint.assistedDisabled']}</div>
    )}

    <div className="lholo-experimental-row">
      <div className="lholo-experimental-label">{radiusLabel}</div>
      <div className="lholo-stepper">
        <LHoloButton disabled={!experimental.ready} label="-" className="lholo-stepper-button"
          onClick={() => experimental.stepPlacementRadius(-1)} />
        <div className="lholo-stepper-value"
          style={{ width: Math.max(3.2, Math.min(8, experimental.placementRadius.text.length * 0.7 + 1.2)) + 'rem' }}>
          <LHoloTextField disabled={!experimental.ready} label={radiusLabel} value={experimental.placementRadius.text} maxLength={4}
            onChange={experimental.editPlacementRadius} onBlur={experimental.commitPlacementRadius} />
        </div>
        <LHoloButton disabled={!experimental.ready} label="+" className="lholo-stepper-button"
          onClick={() => experimental.stepPlacementRadius(1)} />
      </div>
    </div>

    <div className="lholo-experimental-row">
      <div className="lholo-experimental-label">{cooldownLabel}</div>
      <div className="lholo-stepper">
        <LHoloButton disabled={!experimental.ready} label="-" className="lholo-stepper-button"
          onClick={() => experimental.stepAutoPlacementCooldown(-1)} />
        <div className="lholo-stepper-value"
          style={{ width: Math.max(3.2, Math.min(8, experimental.autoPlacementCooldown.text.length * 0.7 + 1.2)) + 'rem' }}>
          <LHoloTextField disabled={!experimental.ready} label={cooldownLabel} value={experimental.autoPlacementCooldown.text} maxLength={4}
            onChange={experimental.editAutoPlacementCooldown} onBlur={experimental.commitAutoPlacementCooldown} />
        </div>
        <LHoloButton disabled={!experimental.ready} label="+" className="lholo-stepper-button"
          onClick={() => experimental.stepAutoPlacementCooldown(1)} />
      </div>
    </div>



    {experimental.pendingConsentFeature !== null && (
      <div className="lholo-modal-backdrop">
        <div className="lholo-modal">
          <div className="lholo-modal-title">{labels['modal.titleExperimental']}</div>
          <div className="lholo-transform-divider" />
          <div className="lholo-modal-body">
            <div className="lholo-modal-paragraph">{labels['experimental.warningIntro']}</div>
            <div className="lholo-modal-paragraph">{labels['experimental.warningAntiCheat']}</div>
            <div className="lholo-modal-paragraph">{labels['experimental.warningAllowed']}</div>
            <div className="lholo-modal-paragraph">{labels['experimental.warningConsequence']}</div>
            <div className="lholo-modal-paragraph">{labels['experimental.warningResponsibility']}</div>
            <div className="lholo-modal-paragraph">{labels['experimental.warningManualSafe']}</div>
          </div>
          <div className="lholo-transform-divider" />
          <div className="lholo-action-row">
            {experimental.experimentalConsent ? (
              <LHoloButton label={labels['button.close']} onClick={experimental.closeModal} />
            ) : <>
              <LHoloButton disabled={!experimental.canCommand} label={labels['button.consentEnable']} className="lholo-danger-button"
                onClick={experimental.confirmConsent} />
              <LHoloButton label={labels['button.cancel']} onClick={experimental.closeModal} />
            </>}
          </div>
        </div>
      </div>
    )}
  </>;
}
