import { useEffect, useState } from 'react'
const fields = [['rate', 'Arrival rate'], ['burst', 'Burst rate'], ['workers', 'Workers'], ['symbols', 'Symbols'], ['high_water', 'High-water'], ['low_water', 'Low-water']]
export default function ControlPanel({ config, status, onSave, onStart, onStop, onReset, onNotice }) {
  const [draft, setDraft] = useState(config || {}); const [busy, setBusy] = useState(false); const [message, setMessage] = useState('')
  useEffect(() => setDraft(config || {}), [config])
  const run = async (action, success = 'Saved') => { setBusy(true); setMessage(''); try { await action(); setMessage(success); onNotice?.(success) } catch (e) { setMessage(e.message); onNotice?.(e.message) } finally { setBusy(false) } }
  return <section className="control-panel panel"><div className="panel-label">Simulation configuration</div><div className="control-grid">
    {fields.map(([key, label]) => <label key={key}>{label}<input type="number" min="1" value={draft[key] ?? ''} onChange={e => setDraft(d => ({...d, [key]: Number(e.target.value)}))} /></label>)}
    <label>Synchronization<select value={draft.sync || 'locked'} onChange={e => setDraft(d => ({...d, sync: e.target.value}))}><option value="locked">Locked</option><option value="lockfree">Lock-free</option></select></label>
    <label>Scheduling policy<select value={draft.policy || 'adaptive'} onChange={e => setDraft(d => ({...d, policy: e.target.value}))}><option value="adaptive">Adaptive</option><option value="priority">Priority</option><option value="sjf">SJF</option><option value="rr">Round robin</option></select></label>
  </div><div className="control-actions"><button disabled={busy} onClick={() => run(() => onSave(draft), 'Settings saved')}>Save settings</button><button className="primary" disabled={busy} onClick={() => run(() => onStart(draft), 'Simulation started')}>Start / apply</button><button disabled={busy || status === 'stopped'} onClick={() => run(onStop, 'Simulation stopped')}>Stop</button><button className="quiet" disabled={busy} onClick={() => run(onReset, 'Defaults restored and simulation started')}>Reset defaults</button>{message && <span className="control-message">{message}</span>}</div></section>
}
