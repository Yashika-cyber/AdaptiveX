import { useRef, useState, useEffect } from 'react'
import MetricHelp from './MetricHelp'

export default function ThroughputCard({ snapshot }) {
  const prevRef = useRef(null)
  const [delta, setDelta] = useState(null)

  useEffect(() => {
    if (!snapshot) return
    if (prevRef.current !== null) {
      setDelta(((snapshot.tput - prevRef.current) / (prevRef.current || 1)) * 100)
    }
    prevRef.current = snapshot.tput
  }, [snapshot?.tput])

  const tput    = snapshot?.tput     ?? 0
  const p50     = snapshot?.p50_us   ?? 0
  const p99     = snapshot?.p99_us   ?? 0
  const qLen    = snapshot?.q_len    ?? 0
  const starved = snapshot?.starved  ?? 0

  const starvedColor = starved > 20 ? 'var(--red)' : starved > 5 ? 'var(--amber)' : 'var(--text-secondary)'

  return (
    <div className="panel">
      <div className="panel-label">Throughput <MetricHelp text="Number of orders completed by the matching engine each second." /></div>
      <p className="card-description">Completed orders per second; use it to compare engine capacity between runs.</p>
      <div className="tput-value">{tput.toLocaleString('en-US', { maximumFractionDigits: 0 })}</div>
      <div className="tput-unit">orders / sec</div>

      {delta !== null && (
        <div className={`tput-delta ${delta >= 0 ? 'up' : 'down'}`}>
          {delta >= 0 ? '↑' : '↓'} {Math.abs(delta).toFixed(1)}%
        </div>
      )}

      <div className="mini-stats">
        <div className="mini-stat">
          <span className="lbl">p50 lat</span>
          <span className="val">{p50.toLocaleString()} µs</span>
        </div>
        <div className="mini-stat">
          <span className="lbl">p99 lat</span>
          <span className="val" style={{ color: p99 > 2000 ? 'var(--amber)' : undefined }}>
            {p99.toLocaleString()} µs
          </span>
        </div>
        <div className="mini-stat">
          <span className="lbl">queue len</span>
          <span className="val" style={{ color: qLen > 400 ? 'var(--amber)' : undefined }}>
            {qLen.toLocaleString()}
          </span>
        </div>
        <div className="mini-stat">
          <span className="lbl">starved</span>
          <span className="val" style={{ color: starvedColor }}>{starved}</span>
        </div>
        <div className="mini-stat"><span className="lbl">avg queue wait</span><span className="val">{(snapshot?.wait_avg_us ?? 0).toFixed(0)} µs</span></div>
        <div className="mini-stat"><span className="lbl">max queue wait</span><span className="val">{snapshot?.wait_max_us ?? 0} µs</span></div>
        <div className="mini-stat"><span className="lbl">CPU usage</span><span className="val">{(snapshot?.cpu_pct ?? 0).toFixed(1)}%</span></div>
        <div className="mini-stat"><span className="lbl">mutex waits</span><span className="val">{snapshot?.lock_contentions ?? 0} · {(snapshot?.lock_wait_us ?? 0).toFixed(1)}µs</span></div>
      </div>
    </div>
  )
}
