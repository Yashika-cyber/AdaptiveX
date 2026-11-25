const TRADERS = [
  { key: 'sr_hft',    label: 'HFT',          color: '#3B82F6' },
  { key: 'sr_retail', label: 'Retail',        color: '#F59E0B' },
  { key: 'sr_mm',     label: 'Market Maker',  color: '#8B5CF6' },
]
import MetricHelp from './MetricHelp'
import { ResponsiveContainer, LineChart, Line, XAxis, YAxis, Tooltip, ReferenceLine } from 'recharts'

function fairColor(j) {
  if (j >= 0.9) return 'var(--green)'
  if (j >= 0.7) return 'var(--amber)'
  return 'var(--red)'
}

export default function FairnessGauge({ snapshot, history }) {
  const j = snapshot?.fairness ?? 0

  return (
    <div className="panel">
      <div className="panel-label">Fairness Index (Jain&apos;s) <MetricHelp text="Jain’s index measures how evenly service is distributed. 1.0 means equal service." /></div>
      <p className="card-description">Shows how evenly completed work is shared across trader types; 1.0 is perfectly balanced.</p>

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
      <div className="fairness-trend-label">FAIRNESS TREND — 1.0 = EQUAL SERVICE</div>
      <div className="fairness-chart"><ResponsiveContainer><LineChart data={(history?.t || []).map((t, i) => ({ t, fairness: history.fairness[i] }))}><XAxis dataKey="t" hide/><YAxis domain={[0, 1.05]} width={30} tick={{fill:'#7BA3C4',fontSize:9}}/><Tooltip/><ReferenceLine y={1} stroke="rgba(16,185,129,.35)" strokeDasharray="3 3"/><Line dataKey="fairness" stroke="#10B981" strokeWidth={3} dot={false}/></LineChart></ResponsiveContainer></div>
    </div>
  )
}
