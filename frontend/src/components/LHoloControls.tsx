import React, { useEffect, useRef, useState } from 'react';
import { NativeInteractive } from '../platform/native';

interface ButtonProps {
  label: string; onClick(): void; disabled?: boolean; className?: string;
  icon?: React.ReactNode;
}

export function LHoloButton({ label, onClick, disabled, className = '', icon }: ButtonProps) {
  return <NativeInteractive label={label} disabled={disabled}
    className={'lholo-button ' + className} onClick={() => { if (!disabled) onClick(); }}>
    <div className="lholo-button-indicator" />
    {icon ? <div className="lholo-button-icon">{icon}</div> : null}
    <div className="lholo-button-label">{label}</div>
  </NativeInteractive>;
}

export function LHoloTextField({ label, value, onChange, disabled, maxLength, onBlur }: {
  label: string; value: string; onChange(value: string): void; disabled?: boolean; maxLength?: number;
  onBlur?(): void;
}) {
  const input = useRef<HTMLInputElement>(null);
  const [editing, setEditing] = useState(false);
  // dom-fiber only forwards a documented subset of JSX event props. Bind the
  // real input event explicitly so keyboard, paste and IME changes all work.
  useEffect(() => {
    const element = input.current;
    if (!element) return;
    const change = () => { if (!disabled) onChange(element.value); };
    element.addEventListener('input', change);
    return () => element.removeEventListener('input', change);
  }, [disabled, onChange]);
  return <NativeInteractive label={label} disabled={disabled}
    className={'lholo-input-shell' + (editing ? ' is-editing' : '')}
    onClick={() => { if (!disabled) input.current?.focus(); }}>
    <input ref={input} className="lholo-text-input" type="text" value={value}
      disabled={disabled} maxLength={maxLength} data-narrate={label}
      onFocus={() => setEditing(true)} onBlur={() => { setEditing(false); onBlur?.(); }}
      onKeyDown={event => { if (event.key === 'Enter' || event.keyCode === 13) input.current?.blur(); }} />
  </NativeInteractive>;
}

export function LHoloCheckbox({ label, value, onChange, disabled }: {
  label: string; value: boolean; onChange(value: boolean): void; disabled?: boolean;
}) {
  return <NativeInteractive label={label} disabled={disabled}
    className={'lholo-checkbox' + (value ? ' is-checked' : '')}
    onClick={() => { if (!disabled) onChange(!value); }}>
    <div className="lholo-checkbox-box">
      {value ? (
        <svg viewBox="0 0 16 16" style={{ width: '0.92rem', height: '0.92rem' }}>
          <path d="M3.2 8.4L6.4 11.6L12.8 4.6" fill="none" stroke="#ffffff"
            strokeWidth="2.1" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
      ) : null}
    </div>
    <div className="lholo-checkbox-label">{label}</div>
  </NativeInteractive>;
}
