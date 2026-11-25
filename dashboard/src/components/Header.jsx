import { useState, useEffect } from 'react'

const POLICY_COLORS = {
  PRIORITY: { bg: 'rgba(139,92,246,.15)', border: 'rgba(139,92,246,.4)', color: '#8B5CF6' },
  SJF:      { bg: 'rgba(59,130,246,.15)',  border: 'rgba(59,130,246,.4)',  color: '#3B82F6' },
  RR:       { bg: 'rgba(16,185,129,.15)',  border: 'rgba(16,185,129,.4)',  color: '#10B981' },
}

const STATUS_LABEL = { live: '● LIVE', connecting: '◌ CONNECTING', error: '✕ ERROR', done: '■ DONE', demo: '◈ DEMO' }
const STATUS_COLOR = { live: '#10B981', connecting: '#F59E0B', error: '#EF4444', done: '#7BA3C4', demo: '#8B5CF6' }

export default function Header({ snapshot, status }) {
  const [elapsed, setElapsed] = useState(0)

  useEffect(() => {
    const id = setInterval(() => setElapsed(e => e + 1), 1000)
    return () => clearInterval(id)
  }, [])

  const policy = snapshot?.policy ?? '—'
  const pc = POLICY_COLORS[policy] || POLICY_COLORS.SJF
  const h  = String(Math.floor(elapsed / 3600)).padStart(2, '0')
  const m  = String(Math.floor((elapsed % 3600) / 60)).padStart(2, '0')
  const s  = String(elapsed % 60).padStart(2, '0')

  return (
    <header style={{
      display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      padding: '10px 20px', borderBottom: '1px solid var(--border)',
      background: 'var(--bg-surface)',
    }}>
      {/* Logo */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <div style={{
          width: 28, height: 28, borderRadius: 6,
          background: 'linear-gradient(135deg,#2563EB,#3B82F6)',
          display: 'flex', alignItems: 'center', justifyContent: 'center',
          fontSize: 14, fontWeight: 700, color: '#fff',
        }}>⚡</div>
        <span style={{ fontWeight: 600, fontSize: 15, letterSpacing: '-.3px' }}>AdaptiveX</span>
        <span style={{ color: 'var(--text-muted)', fontSize: 11, fontFamily: 'var(--font-mono)' }}>
          Order-Matching Engine
        </span>
      </div>

      {/* Status row */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 14, fontFamily: 'var(--font-mono)' }}>
        {/* Live / status badge */}
        <span style={{
          fontSize: 11, fontWeight: 600,
          color: STATUS_COLOR[status] || '#7BA3C4',
        }}>
          {STATUS_LABEL[status] || status}
        </span>

        {/* Active policy */}
        <span style={{
          background: pc.bg, border: `1px solid ${pc.border}`, color: pc.color,
          padding: '3px 10px', borderRadius: 4, fontSize: 11, fontWeight: 600, letterSpacing: '.5px',
        }}>
          {policy}
        </span>

        {/* Switches */}
        {snapshot && (
          <span style={{ color: 'var(--text-secondary)', fontSize: 11 }}>
            {snapshot.switches} switch{snapshot.switches !== 1 ? 'es' : ''}
          </span>
        )}

        {/* Clock */}
        <span style={{ color: 'var(--text-muted)', fontSize: 11, minWidth: 60 }}>
          {h}:{m}:{s}
        </span>
      </div>
    </header>
  )
}
