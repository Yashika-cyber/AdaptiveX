import { useEffect, useState } from 'react'
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
import WorkerHeatmap from './components/WorkerHeatmap'
import RunComparison from './components/RunComparison'
import InsightsPanel from './components/InsightsPanel'
import ExperimentLab from './components/ExperimentLab'
import './index.css'

export default function App() {
  const [tab, setTab] = useState('overview')
  const [toast, setToast] = useState(null)
  const [age, setAge] = useState(null)
  const metrics = useMetrics()
  const { snapshot, history, status, updatedAt } = metrics
  useEffect(() => {
    const timer = setInterval(() => setAge(updatedAt ? Math.floor((Date.now() - updatedAt) / 1000) : null), 1000)
    return () => clearInterval(timer)
  }, [updatedAt])
  useEffect(() => { if (!toast) return; const timer = setTimeout(() => setToast(null), 3200); return () => clearTimeout(timer) }, [toast])
  const notify = message => setToast(message)
  const control = <ControlPanel config={metrics.config} status={status} onSave={metrics.saveConfig} onStart={metrics.start} onStop={metrics.stop} onReset={metrics.reset} onNotice={notify} />
  const setBookSymbol = async symbol => {
    try { await metrics.start({ ...(metrics.config || {}), book_symbol: symbol }); notify(`Order book switched to ${symbol}`) }
    catch (error) { notify(error.message) }
  }

  return (
    <div className="app">
      <Header snapshot={snapshot} status={status} />

      {snapshot?.burst && (
        <div className="burst-banner">⚠ BURST MODE ACTIVE</div>
      )}

      <nav className="tab-bar">{[['overview','01. Overview'],['performance','02. Performance'],['fairness','03. Fairness'],['internals','04. Engine internals'],['book','05. Order book'],['compare','06. Compare'],['lab','07. Experiment Lab']].map(([id, label]) => <button key={id} className={tab === id ? 'active' : ''} onClick={() => setTab(id)}>{label}</button>)}</nav>
      <main className="dashboard-content">
        <p className="page-description">{tab === 'overview' ? 'Live system health, policy activity and key matching metrics.' : tab === 'performance' ? 'Latency percentiles, throughput and queue behaviour, including per-worker activity.' : tab === 'fairness' ? 'Jain’s index, trader service ratios and starvation.' : tab === 'internals' ? 'Predictive cache, LRU and durable WAL metrics.' : tab === 'book' ? 'Aggregated bid and ask depth streamed from the engine.' : tab === 'compare' ? 'Compare measured throughput and p99 latency across policies and synchronization modes.' : 'Run repeatable benchmark suites, review historical summaries, and export CSV reports.'}<span>{age === null ? 'Waiting for first snapshot' : `Updated ${age}s ago`}</span></p>
        {!snapshot ? <div className="page-state">{status === 'error' ? 'Connection error — retrying…' : 'Waiting for engine metrics…'}</div> : <>
        {tab === 'overview' && <><div className="main-grid"><ThroughputCard snapshot={snapshot}/><LatencyChart snapshot={snapshot} history={history}/><FairnessGauge snapshot={snapshot}/><PolicyTimeline snapshot={snapshot}/></div>{control}</>}
        {tab === 'performance' && <><div className="main-grid two-columns"><LatencyChart snapshot={snapshot} history={history}/><ThroughputCard snapshot={snapshot}/><WorkerHeatmap snapshot={snapshot}/><PolicyTimeline snapshot={snapshot}/></div>{control}</>}
        {tab === 'fairness' && <div className="main-grid single-column"><InsightsPanel snapshot={snapshot} type="fairness"/><FairnessGauge snapshot={snapshot} history={history}/><PolicyTimeline snapshot={snapshot}/></div>}
        {tab === 'internals' && <div className="main-grid two-columns"><InsightsPanel snapshot={snapshot} type="internals"/><CacheMetrics snapshot={snapshot}/><WALMetrics snapshot={snapshot}/></div>}
        {tab === 'book' && <div className="main-grid single-column"><OrderBookViz snapshot={snapshot} config={metrics.config} onSymbolChange={setBookSymbol}/></div>}
        {tab === 'compare' && <div className="main-grid single-column"><RunComparison snapshot={snapshot} config={metrics.config} onRun={async sync => { try { await metrics.start({ ...(metrics.config || {}), sync }); notify(`${sync === 'locked' ? 'Mutex / Locked' : 'Lock-free'} run started`) } catch (error) { notify(error.message) } }}/></div>}
        {tab === 'lab' && <ExperimentLab/>}
        </>}
      </main>
      {toast && <div className="toast" role="status">{toast}</div>}
    </div>
  )
}
