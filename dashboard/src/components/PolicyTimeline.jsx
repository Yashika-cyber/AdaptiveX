import { useState, useEffect, useRef } from 'react'
import MetricHelp from './MetricHelp'

const MAX_HISTORY = 60  // seconds to show

export default function PolicyTimeline({ snapshot }) {
  const [segments, setSegments] = useState([])  // [{policy, duration}]
  const lastPolicy = useRef(null)

  useEffect(() => {
    if (!snapshot?.policy) return
    const p = snapshot.policy

    setSegments(prev => {
      if (prev.length === 0 || lastPolicy.current !== p) {
        lastPolicy.current = p
        const next = [...prev, { policy: p, duration: 1 }]
        // Trim to MAX_HISTORY total seconds
        let total = next.reduce((s, x) => s + x.duration, 0)
        while (total > MAX_HISTORY && next.length > 1) {
          total -= next[0].duration
          next.shift()
        }
        return next
      }
      // Extend last segment
      const next = [...prev]
      next[next.length - 1] = { ...next[next.length - 1], duration: next[next.length - 1].duration + 1 }
      let total = next.reduce((s, x) => s + x.duration, 0)
      while (total > MAX_HISTORY && next.length > 1) {
        total -= next[0].duration
        next.shift()
      }
      return next
    })
  }, [snapshot?.t])

  const total = segments.reduce((s, x) => s + x.duration, 0) || 1

  const counts = { PRIORITY: 0, SJF: 0, RR: 0 }
  segments.forEach(s => { counts[s.policy] = (counts[s.policy] || 0) + s.duration })

  return (
    <div className="panel panel-timeline">
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
        <div className="panel-label" style={{ marginBottom: 0 }}>Policy Timeline — last {total}s <MetricHelp text="Shows the active scheduling policy over time. Adaptive mode can switch policy as queue pressure changes." /></div>
        <div style={{ display: 'flex', gap: 16 }}>
          {Object.entries(counts).filter(([,v]) => v > 0).map(([p, v]) => (
            <span key={p} style={{
              fontFamily: 'var(--font-mono)', fontSize: 10,
              color: p === 'SJF' ? 'var(--blue)' : p === 'PRIORITY' ? 'var(--purple)' : 'var(--green)',
            }}>
              {p}: {v}s ({((v/total)*100).toFixed(0)}%)
            </span>
          ))}
        </div>
      </div>
      <p className="card-description">Tracks which scheduling policy processed the queue over the recent run.</p>

      <div className="timeline-track">
        {segments.map((seg, i) => (
          <div
            key={i}
            className={`timeline-seg seg-${seg.policy}`}
            style={{ flex: seg.duration }}
            title={`${seg.policy}: ${seg.duration}s`}
          >
            {(seg.duration / total) > 0.07 ? seg.policy : ''}
          </div>
        ))}
        {segments.length === 0 && (
          <div style={{
            flex: 1, background: 'var(--bg-elevated)', display: 'flex',
            alignItems: 'center', justifyContent: 'center',
            fontFamily: 'var(--font-mono)', fontSize: 10, color: 'var(--text-muted)',
          }}>
            waiting for data…
          </div>
        )}
      </div>
    </div>
  )
}
