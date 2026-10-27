import React from 'react';

export function SelectedFileContext({label, name, className = ''}: {
  label: string; name: string; className?: string;
}) {
  return <div className={`lholo-selected-file-context ${className}`.trim()}>
    <span className="lholo-selected-file-label">{`${label}:`}</span>
    <span className="lholo-selected-file-name">{name}</span>
  </div>;
}
