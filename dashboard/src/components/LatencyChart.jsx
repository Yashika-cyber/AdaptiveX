import {
  ResponsiveContainer, AreaChart, Area, XAxis, YAxis,
  CartesianGrid, Tooltip, Legend,
} from 'recharts'
import MetricHelp from './MetricHelp'

const FMT = (v) => v ? v.toLocaleString() + 'µs' : '—'

function CustomTooltip({ active, payload, label }) {
  if (!active || !payload?.length) return null
  return (
    <div style={{
      background: 'var(--bg-elevated)', border: '1px solid var(--border)',
      padding: '6px 10px', borderRadius: 4, fontFamily: 'var(--font-mono)', fontSize: 11,
    }}>
      <div style={{ color: 'var(--text-muted)', marginBottom: 4 }}>t={label}s</div>
      {payload.map(p => (
        <div key={p.dataKey} style={{ color: p.stroke }}>
          {p.dataKey}: {p.value?.toLocaleString()} µs
        </div>
      ))}
    </div>
  )
}

export default function LatencyChart({ snapshot, history }) {
  const data = history.t.map((t, i) => ({
    t, p50: history.p50[i], p90: history.p90[i], p95: history.p95[i], p99: history.p99[i],
  }))

  const p50 = snapshot?.p50_us ?? 0
  const p90 = snapshot?.p90_us ?? 0
  const p95 = snapshot?.p95_us ?? 0
  const p99 = snapshot?.p99_us ?? 0

  return (
    <div className="panel">
      <div className="panel-label">Latency <MetricHelp text="Time from entering the engine queue to order completion, measured in microseconds." /></div>
      <p className="card-description">Queue-to-completion time. p50 is typical latency; p99 highlights slow tail behaviour.</p>
      <div style={{ height: 200 }}>
        <ResponsiveContainer width="100%" height="100%">
          <AreaChart data={data} margin={{ top: 4, right: 4, bottom: 0, left: 0 }}>
            <defs>
              <linearGradient id="g99" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor="#EF4444" stopOpacity={0.25} />
                <stop offset="100%" stopColor="#EF4444" stopOpacity={0} />
              </linearGradient>
              <linearGradient id="g50" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor="#3B82F6" stopOpacity={0.25} />
                <stop offset="100%" stopColor="#3B82F6" stopOpacity={0} />
              </linearGradient>
            </defs>
            <CartesianGrid strokeDasharray="3 3" stroke="rgba(30,58,95,.5)" vertical={false} />
            <XAxis dataKey="t" hide />
            <YAxis
              tick={{ fill: '#3D6080', fontSize: 9, fontFamily: 'var(--font-mono)' }}
              tickFormatter={v => v > 999 ? (v/1000).toFixed(1)+'ms' : v+'µs'}
              width={42}
              axisLine={false}
              tickLine={false}
            />
            <Tooltip content={<CustomTooltip />} />
            <Area type="monotone" dataKey="p99" stroke="#EF4444" strokeWidth={1.5} fill="url(#g99)" dot={false} />
            <Area type="monotone" dataKey="p95" stroke="#F59E0B" strokeWidth={1.25} fill="none" dot={false} />
            <Area type="monotone" dataKey="p90" stroke="#8B5CF6" strokeWidth={1.25} fill="none" dot={false} />
            <Area type="monotone" dataKey="p50" stroke="#3B82F6" strokeWidth={2}   fill="url(#g50)" dot={false} />
          </AreaChart>
        </ResponsiveContainer>
      </div>

      <div className="chart-legend">
        <div className="legend-item">
          <div className="legend-line" style={{ background: '#3B82F6' }} />
          p50 <strong style={{ color: 'var(--text-primary)', marginLeft: 4 }}>{FMT(p50)}</strong>
        </div>
        <div className="legend-item"><div className="legend-line" style={{ background: '#8B5CF6' }} />p90 <strong>{FMT(p90)}</strong></div>
        <div className="legend-item"><div className="legend-line" style={{ background: '#F59E0B' }} />p95 <strong>{FMT(p95)}</strong></div>
        <div className="legend-item">
          <div className="legend-line" style={{ background: '#EF4444' }} />
          p99 <strong style={{ color: 'var(--text-primary)', marginLeft: 4 }}>{FMT(p99)}</strong>
        </div>
      </div>
    </div>
  )
}
