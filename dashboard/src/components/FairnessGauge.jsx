const TRADERS = [
  { key: 'sr_hft',    label: 'HFT',          color: '#3B82F6' },
  { key: 'sr_retail', label: 'Retail',        color: '#F59E0B' },
  { key: 'sr_mm',     label: 'Market Maker',  color: '#8B5CF6' },
]

function fairColor(j) {
  if (j >= 0.9) return 'var(--green)'
  if (j >= 0.7) return 'var(--amber)'
  return 'var(--red)'
}

export default function FairnessGauge({ snapshot }) {
  const j = snapshot?.fairness ?? 0

  return (
    <div className="panel">
      <div className="panel-label">Fairness Index (Jain&apos;s)</div>

      <div className="fair-index" style={{ color: fairColor(j) }}>
        {j.toFixed(3)}
      </div>
      <div className="fair-bar-bg">
        <div className="fair-bar-fill" style={{ width: `${j * 100}%` }} />
      </div>

      <div className="trader-rows">
        {TRADERS.map(t => {
          const pct = ((snapshot?.[t.key] ?? 0) * 100)
          return (
            <div key={t.key}>
              <div className="trader-name">{t.label} — service ratio</div>
              <div className="trader-bar-bg">
                <div className="trader-bar-fill"
                  style={{ width: `${pct}%`, background: t.color }} />
              </div>
              <div className="trader-pct">{pct.toFixed(1)}%</div>
            </div>
          )
        })}
      </div>

      {(snapshot?.starved ?? 0) > 0 && (
        <div style={{
          marginTop: 12, fontFamily: 'var(--font-mono)', fontSize: 10,
          color: 'var(--amber)',
        }}>
          ⚠ {snapshot.starved} orders waited &gt;10× median
        </div>
      )}
    </div>
  )
}
