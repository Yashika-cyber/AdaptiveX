export default function CacheMetrics({ snapshot }) {
  const pred = ((snapshot?.cache_pred ?? 0) * 100)
  const lru  = ((snapshot?.cache_lru  ?? 0) * 100)
  const diff = (pred - lru).toFixed(1)

  return (
    <div className="panel">
      <div className="panel-label">Cache Comparison</div>
      <p className="card-description">Hit rate for the predictive cache compared with the LRU baseline.</p>
      <div className="cache-compare">
        <div>
          <div className="cache-row-label">
            <span className="cache-name-txt">Predictive (Markov+EWMA)</span>
            <span className="cache-pct-txt">{pred.toFixed(1)}%</span>
          </div>
          <div className="cache-bar-bg">
            <div className="cache-bar-fill cache-pred-fill" style={{ width: `${pred}%` }} />
          </div>
        </div>

        <div>
          <div className="cache-row-label">
            <span className="cache-name-txt">LRU (baseline)</span>
            <span className="cache-pct-txt">{lru.toFixed(1)}%</span>
          </div>
          <div className="cache-bar-bg">
            <div className="cache-bar-fill cache-lru-fill" style={{ width: `${lru}%` }} />
          </div>
        </div>
      </div>

      {snapshot && (
        <div className="cache-advantage">
          {parseFloat(diff) >= 0
            ? `↑ Predictive wins by ${diff}pp`
            : `↓ LRU ahead by ${Math.abs(parseFloat(diff)).toFixed(1)}pp`}
        </div>
      )}

      <div style={{ marginTop: 10, fontFamily: 'var(--font-mono)', fontSize: 10, color: 'var(--text-muted)', lineHeight: 1.5 }}>
        cap: 50 symbols<br />
        prefetch top-5 predicted
      </div>
    </div>
  )
}
