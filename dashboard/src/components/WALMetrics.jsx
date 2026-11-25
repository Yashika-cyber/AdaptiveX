export default function WALMetrics({ snapshot }) {
  const critMs = snapshot?.wal_crit_ms ?? 0
  const routMs = snapshot?.wal_rout_ms ?? 0
  const lift   = snapshot?.wal_lift   ?? 0

  const critCells = Math.min(8, Math.round((snapshot?.tput ?? 0) / 400))
  const routCells = Math.min(20 - critCells, Math.round((snapshot?.tput ?? 0) / 200))

  return (
    <div className="panel">
      <div className="panel-label">Write-Ahead Log</div>
      <div className="wal-stats">
        <div className="wal-row">
          <span className="wal-lbl">critical lat.</span>
          <span className="wal-val" style={{ color: critMs > 2 ? 'var(--amber)' : undefined }}>
            {critMs.toFixed(2)} ms
          </span>
        </div>
        <div className="wal-row">
          <span className="wal-lbl">routine lat.</span>
          <span className="wal-val">{routMs.toFixed(2)} ms</span>
        </div>
        <div className="wal-row">
          <span className="wal-lbl">priority lift</span>
          <span className="wal-val" style={{ color: 'var(--green)' }}>
            {lift.toFixed(1)}×
          </span>
        </div>
        <div className="wal-row">
          <span className="wal-lbl">flush count</span>
          <span className="wal-val">{(snapshot?.t ?? 0) * 6}/s est.</span>
        </div>
      </div>

      <div className="wal-queue-label">QUEUE DEPTH — CRITICAL | ROUTINE</div>
      <div className="wal-queue-vis">
        {Array.from({ length: 20 }, (_, i) => {
          const isCrit = i < critCells
          const isRout = i >= critCells && i < critCells + routCells
          const bg = isCrit ? 'var(--red)'
                   : isRout ? 'rgba(100,120,150,.35)'
                   : 'rgba(30,58,95,.3)'
          return <div key={i} className="wal-cell" style={{ background: bg }} />
        })}
      </div>

      <div style={{ marginTop: 10, fontFamily: 'var(--font-mono)', fontSize: 10, color: 'var(--text-muted)', lineHeight: 1.5 }}>
        critical → flush immediately<br />
        routine → batch every 10ms
      </div>
    </div>
  )
}
