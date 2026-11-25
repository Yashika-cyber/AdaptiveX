function fmtQty(n) { return n >= 1000 ? (n/1000).toFixed(1)+'k' : String(n) }

export default function OrderBookViz({ snapshot }) {
  const book = snapshot?.book
  const bids = book?.bids || []
  const asks = book?.asks || []
  const maxQty = Math.max(1, ...bids.map(([, qty]) => qty), ...asks.map(([, qty]) => qty))
  const spread = bids[0] && asks[0] ? (asks[0][0] - bids[0][0]).toFixed(2) : '—'

  return (
    <div className="panel">
      <div className="panel-label">
        Live Order Book — <span className="book-sym">{book?.symbol || 'waiting'}</span>
      </div>
      <p className="card-description">Current queued buy and sell quantity at the best five price levels for this symbol.</p>
      {!book && <div className="empty-state">Waiting for real engine depth data…</div>}
      {book && <>
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
          {[...asks].reverse().map(([price, qty], i) => (
            <tr key={`ask-${i}`}>
              <td style={{ color: 'var(--text-muted)' }}>—</td>
              <td className="book-price" style={{ textAlign: 'center' }}>
                ${price.toFixed(2)}
                <div style={{
                  position: 'absolute', right: 0, top: 0, bottom: 0,
                  width: `${(qty / maxQty) * 55}%`,
                  background: 'rgba(239,68,68,.07)',
                  pointerEvents: 'none',
                }} />
              </td>
              <td className="ask-qty">{fmtQty(qty)}</td>
            </tr>
          ))}

          {/* Spread row */}
          <tr>
            <td colSpan={3} className="book-spread">
              spread ${spread} · aggregated price levels
            </td>
          </tr>

          {/* Bids — best bid closest to spread */}
          {bids.map(([price, qty], i) => (
            <tr key={`bid-${i}`} style={{ position: 'relative' }}>
              <td className="bid-qty">{fmtQty(qty)}</td>
              <td className="book-price" style={{ textAlign: 'center' }}>
                ${price.toFixed(2)}
                <div style={{
                  position: 'absolute', left: 0, top: 0, bottom: 0,
                  width: `${(qty / maxQty) * 55}%`,
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
        Depth levels streamed from the engine snapshot
      </div>
      </>}
    </div>
  )
}
