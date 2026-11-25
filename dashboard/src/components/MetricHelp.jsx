import { useRef, useState } from 'react'

export default function MetricHelp({ text }) {
  const ref = useRef(null)
  const [position, setPosition] = useState(null)
  const show = () => {
    const rect = ref.current?.getBoundingClientRect()
    if (!rect) return
    const width = Math.min(280, window.innerWidth - 32)
    setPosition({ left: Math.max(16, Math.min(rect.left, window.innerWidth - width - 16)), top: Math.min(rect.bottom + 10, window.innerHeight - 100), width })
  }
  return <span ref={ref} className="metric-help" tabIndex="0" role="button" aria-label={`About this metric: ${text}`} onMouseEnter={show} onMouseLeave={() => setPosition(null)} onFocus={show} onBlur={() => setPosition(null)}>ⓘ
    {position && <span className="metric-tooltip" style={position}>About this metric: {text}</span>}
  </span>
}
