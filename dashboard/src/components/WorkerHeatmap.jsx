import MetricHelp from './MetricHelp'
export default function WorkerHeatmap({ snapshot }) {
  const workers = snapshot?.workers || []; const max = Math.max(1, ...workers)
  return <div className="panel worker-panel"><div className="panel-label">Worker activity <MetricHelp text="Orders completed by each matching worker during the most recent one-second snapshot." /></div><p className="card-description">Darker cells show workers that processed more orders in the latest interval.</p><div className="heatmap">{workers.map((count, i) => <div key={i} className="heat-cell" style={{background: `rgba(59,130,246,${.15 + .85 * count / max})`}}><b>W{i + 1}</b><span>{count}</span></div>)}</div></div>
}
