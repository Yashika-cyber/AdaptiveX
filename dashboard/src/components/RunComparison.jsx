import { useState } from 'react'
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, Legend } from 'recharts'
import MetricHelp from './MetricHelp'
export default function RunComparison({ snapshot, config, onRun }) {
  const [runs, setRuns] = useState([])
  const label = `${snapshot?.policy || 'Run'} · ${config?.sync === 'lockfree' ? 'Lock-free' : 'Mutex'}`
  const capture = () => setRuns(prev => [...prev.filter(r => r.label !== label), { label, throughput: snapshot.tput, p99: snapshot.p99_us }])
  return <div className="panel"><div className="panel-label">Run comparison <MetricHelp text="Capture a live run under each policy or synchronization mode to compare real measured results." /></div><p className="card-description">Choose a mode, let it run briefly, then capture it. The graph only uses measured runs.</p><div className="comparison-actions"><button className="capture-button" onClick={() => onRun('locked')}>Run Mutex / Locked</button><button className="capture-button" onClick={() => onRun('lockfree')}>Run Lock-free</button><button className="capture-button secondary" onClick={capture}>Capture {label}</button></div>{runs.length ? <div className="comparison-chart"><ResponsiveContainer><BarChart data={runs}><XAxis dataKey="label"/><YAxis yAxisId="left"/><YAxis yAxisId="right" orientation="right"/><Tooltip/><Legend/><Bar yAxisId="left" dataKey="throughput" fill="#3B82F6" name="orders/sec"/><Bar yAxisId="right" dataKey="p99" fill="#EF4444" name="p99 µs"/></BarChart></ResponsiveContainer></div> : <div className="empty-state">Run and capture both Mutex and Lock-free to populate the comparison.</div>}</div>
}
