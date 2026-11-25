import { useState, useEffect, useRef, useCallback } from 'react'

const WS_URL = import.meta.env.VITE_WS_URL || 'ws://localhost:8000/ws'
const HISTORY_LEN = 90  // seconds of history to keep

export function useMetrics() {
  const [snapshot, setSnapshot] = useState(null)
  const [history,  setHistory]  = useState({ p50: [], p99: [], tput: [], t: [] })
  const [status,   setStatus]   = useState('connecting')  // connecting | live | demo | error
  const wsRef = useRef(null)
  const retryRef = useRef(null)

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return
    try {
      const ws = new WebSocket(WS_URL)
      wsRef.current = ws

      ws.onopen  = () => setStatus('live')
      ws.onerror = () => setStatus('error')
      ws.onclose = () => {
        setStatus('connecting')
        retryRef.current = setTimeout(connect, 2000)
      }
      ws.onmessage = (e) => {
        try {
          const msg = JSON.parse(e.data)
          if (msg.type === 'mode') setStatus(msg.data.mode)
          if (msg.type === 'snapshot') {
            const d = msg.data
            setSnapshot(d)
            setHistory(h => {
              const push = (arr, v) => [...arr, v].slice(-HISTORY_LEN)
              return {
                p50:  push(h.p50,  d.p50_us),
                p99:  push(h.p99,  d.p99_us),
                tput: push(h.tput, d.tput),
                t:    push(h.t,    d.t),
              }
            })
          }
          if (msg.type === 'done') setStatus('done')
        } catch (_) {}
      }
    } catch (_) {
      setStatus('error')
      retryRef.current = setTimeout(connect, 3000)
    }
  }, [])

  useEffect(() => {
    const connectTimer = setTimeout(connect, 0)
    return () => {
      clearTimeout(connectTimer)
      clearTimeout(retryRef.current)
      if (wsRef.current) {
        wsRef.current.onclose = null
        wsRef.current.onopen = null
        wsRef.current.onmessage = null
        wsRef.current.onerror = null
        wsRef.current.close()
        wsRef.current = null
      }
    }
  }, [connect])

  return { snapshot, history, status }
}
