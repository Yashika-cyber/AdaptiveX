import { useState } from 'react'
import { useMetrics } from './hooks/useMetrics'
import Header         from './components/Header'
import LatencyChart   from './components/LatencyChart'
import ThroughputCard from './components/ThroughputCard'
import FairnessGauge  from './components/FairnessGauge'
import PolicyTimeline from './components/PolicyTimeline'
import CacheMetrics   from './components/CacheMetrics'
import WALMetrics     from './components/WALMetrics'
import OrderBookViz   from './components/OrderBookViz'
import ControlPanel from './components/ControlPanel'
import './index.css'

export default function App() {
  const [tab, setTab] = useState('overview')
  const metrics = useMetrics()
  const { snapshot, history, status } = metrics
  const control = <ControlPanel config={metrics.config} status={status} onSave={metrics.saveConfig} onStart={metrics.start} onStop={metrics.stop} onReset={metrics.reset} />

  return (
    <div className="app">
      <Header snapshot={snapshot} status={status} />

      {snapshot?.burst && (
        <div className="burst-banner">⚠ BURST MODE ACTIVE</div>
      )}

      <nav className="tab-bar">{[['overview','01. Overview'],['performance','02. Performance'],['fairness','03. Fairness'],['internals','04. Engine internals'],['book','05. Order book']].map(([id, label]) => <button key={id} className={tab === id ? 'active' : ''} onClick={() => setTab(id)}>{label}</button>)}</nav>
      <main className="dashboard-content">
        <p className="page-description">{tab === 'overview' ? 'Live system health, policy activity and key matching metrics.' : tab === 'performance' ? 'Latency percentiles, throughput and queue behaviour.' : tab === 'fairness' ? 'Jain’s index, trader service ratios and starvation.' : tab === 'internals' ? 'Predictive cache, LRU and durable WAL metrics.' : 'Aggregated bid and ask depth streamed from the engine.'}<span>Snapshot {new Date().toLocaleTimeString()}</span></p>
        {tab === 'overview' && <><div className="main-grid"><ThroughputCard snapshot={snapshot}/><LatencyChart snapshot={snapshot} history={history}/><FairnessGauge snapshot={snapshot}/><PolicyTimeline snapshot={snapshot}/></div>{control}</>}
        {tab === 'performance' && <><div className="main-grid two-columns"><LatencyChart snapshot={snapshot} history={history}/><ThroughputCard snapshot={snapshot}/><PolicyTimeline snapshot={snapshot}/></div>{control}</>}
        {tab === 'fairness' && <div className="main-grid single-column"><FairnessGauge snapshot={snapshot}/><PolicyTimeline snapshot={snapshot}/></div>}
        {tab === 'internals' && <div className="main-grid two-columns"><CacheMetrics snapshot={snapshot}/><WALMetrics snapshot={snapshot}/></div>}
        {tab === 'book' && <div className="main-grid single-column"><OrderBookViz snapshot={snapshot}/></div>}
      </main>
    </div>
  )
}
