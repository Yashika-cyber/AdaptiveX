import { useMetrics } from './hooks/useMetrics'
import Header         from './components/Header'
import LatencyChart   from './components/LatencyChart'
import ThroughputCard from './components/ThroughputCard'
import FairnessGauge  from './components/FairnessGauge'
import PolicyTimeline from './components/PolicyTimeline'
import CacheMetrics   from './components/CacheMetrics'
import WALMetrics     from './components/WALMetrics'
import OrderBookViz   from './components/OrderBookViz'
import './index.css'

export default function App() {
  const { snapshot, history, status } = useMetrics()

  return (
    <div className="app">
      <Header snapshot={snapshot} status={status} />

      {snapshot?.burst && (
        <div className="burst-banner">⚠ BURST MODE ACTIVE</div>
      )}

      <div className="main-grid">
        {/* Row 1: Throughput | Latency chart | Fairness */}
        <ThroughputCard snapshot={snapshot} />
        <LatencyChart   snapshot={snapshot} history={history} />
        <FairnessGauge  snapshot={snapshot} />

        {/* Row 2: Policy timeline full-width */}
        <PolicyTimeline snapshot={snapshot} />

        {/* Row 3: Cache | WAL | Order Book */}
        <CacheMetrics   snapshot={snapshot} />
        <WALMetrics     snapshot={snapshot} />
        <OrderBookViz   snapshot={snapshot} />
      </div>
    </div>
  )
}
