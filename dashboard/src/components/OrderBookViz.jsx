import { useState, useEffect } from 'react'

function fmtQty(n) { return n >= 1000 ? (n/1000).toFixed(1)+'k' : String(n) }

function genBook(mid) {
  const bids = Array.from({ length: 5 }, (_, i) => ({
    price: mid - (i + 1) * 0.01 - Math.random() * 0.005,
    qty:   Math.round(300 + Math.random() * 3500),
  }))
  const asks = Array.from({ length: 5 }, (_, i) => ({
    price: mid + (i + 1) * 0.01 + Math.random() * 0.005,
    qty:   Math.round(300 + Math.random() * 3500),
  }))
  return { bids, asks }
}

const BASE_MID = 182.45

export default function OrderBookViz({ snapshot }) {
  const [book, setBook] = useState(() => genBook(BASE_MID))
  const [symIdx, setSymIdx] = useState(0)
  const SYMBOLS = ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'TSLA']

  // Regenerate book on every snapshot tick
  useEffect(() => {
    if (!snapshot) return
    const drift = (snapshot.t ?? 0) * 0.002 + Math.sin((snapshot.t ?? 0) * 0.1) * 0.3
    setBook(genBook(BASE_MID + drift))
    setSymIdx(Math.floor((snapshot.t ?? 0) / 10) % SYMBOLS.length)
  }, [snapshot?.t])

  const { bids, asks } = book
  const sym = SYMBOLS[symIdx]
  const maxQty = Math.max(...bids.map(b => b.qty), ...asks.map(a => a.qty))
  const spread = asks[0] ? (asks[0].price - bids[0].price).toFixed(2) : '—'

  return (
    <div className="panel">
      <div className="panel-label">
        Illustrative Order Book — <span className="book-sym">{sym}</span>
      </div>

      <table className="book-table">
        <thead>
          <tr>
            <th style={{ textAlign: 'left' }}>QTY (BID)</th>
            <th style={{ textAlign: 'center' }}>PRICE</th>
            <th style={{ textAlign: 'right' }}>QTY (ASK)</th>
          </tr>
        </thead>
        <tbody>
          {/* Asks reversed — best ask closest to spread */}
          {[...asks].reverse().map((a, i) => (
            <tr key={`ask-${i}`}>
              <td style={{ color: 'var(--text-muted)' }}>—</td>
              <td className="book-price" style={{ textAlign: 'center' }}>
                ${a.price.toFixed(2)}
                <div style={{
                  position: 'absolute', right: 0, top: 0, bottom: 0,
                  width: `${(a.qty / maxQty) * 55}%`,
                  background: 'rgba(239,68,68,.07)',
                  pointerEvents: 'none',
                }} />
              </td>
              <td className="ask-qty">{fmtQty(a.qty)}</td>
            </tr>
          ))}

          {/* Spread row */}
          <tr>
            <td colSpan={3} className="book-spread">
              spread ${spread} · {sym}
            </td>
          </tr>

          {/* Bids — best bid closest to spread */}
          {bids.map((b, i) => (
            <tr key={`bid-${i}`} style={{ position: 'relative' }}>
              <td className="bid-qty">{fmtQty(b.qty)}</td>
              <td className="book-price" style={{ textAlign: 'center' }}>
                ${b.price.toFixed(2)}
                <div style={{
                  position: 'absolute', left: 0, top: 0, bottom: 0,
                  width: `${(b.qty / maxQty) * 55}%`,
                  background: 'rgba(16,185,129,.07)',
                  pointerEvents: 'none',
                }} />
              </td>
              <td style={{ color: 'var(--text-muted)' }}>—</td>
            </tr>
          ))}
        </tbody>
      </table>
      <div style={{ marginTop: 8, color: 'var(--text-muted)', fontSize: 10 }}>
        Sample levels; not streamed by the engine
      </div>
    </div>
  )
}
