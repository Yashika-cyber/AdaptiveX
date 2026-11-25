import MetricHelp from './MetricHelp'
export default function InsightsPanel({ snapshot, type }) {
  const fairness = type === 'fairness'
  const title = fairness ? 'Fairness summary' : 'Engine health summary'
  const score = snapshot?.fairness ?? 0
  const cacheLead = ((snapshot?.cache_pred ?? 0) - (snapshot?.cache_lru ?? 0)) * 100
  const message = fairness
    ? score >= .9 ? 'Service is evenly distributed across trader types.' : 'Service distribution needs attention; inspect the weakest trader ratio.'
    : cacheLead >= 0 ? `Predictive cache leads LRU by ${cacheLead.toFixed(1)} percentage points.` : `LRU currently leads predictive cache by ${Math.abs(cacheLead).toFixed(1)} percentage points.`
  return <div className="panel insights-panel"><div className="panel-label">{title} <MetricHelp text="A concise interpretation of the metrics on this tab." /></div><p className="insight-status">{message}</p><div className="insight-grid">{fairness ? <><div><span>Jain’s index</span><b>{score.toFixed(3)}</b></div><div><span>Starved orders</span><b className={snapshot?.starved ? 'warn' : ''}>{snapshot?.starved ?? 0}</b></div><div><span>Queue depth</span><b>{snapshot?.q_len ?? 0}</b></div></> : <><div><span>Predictive cache</span><b>{((snapshot?.cache_pred ?? 0) * 100).toFixed(1)}%</b></div><div><span>WAL flushes</span><b>{snapshot?.wal_flushes ?? 0}</b></div><div><span>Critical log latency</span><b>{(snapshot?.wal_crit_ms ?? 0).toFixed(2)} ms</b></div></>}</div></div>
}
