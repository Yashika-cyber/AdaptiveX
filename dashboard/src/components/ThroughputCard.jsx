import { useRef, useState, useEffect } from 'react'

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
      <div className="panel-label">Throughput</div>
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
      </div>
    </div>
  )
}
