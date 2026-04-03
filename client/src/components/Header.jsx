import { useState, useEffect } from 'react'

export default function Header({ cattleList, activeCattle, onCattleChange, isOnline }) {
  const [time, setTime] = useState('')

  useEffect(() => {
    const tick = () => {
      setTime(new Date().toLocaleTimeString('en-US', {
        hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false,
      }))
    }
    tick()
    const id = setInterval(tick, 1000)
    return () => clearInterval(id)
  }, [])

  return (
    <header className="dash-header">
      <div className="header-left">
        <div className="logo-icon">
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2"/>
            <path d="M12 8v4l3 3"/>
          </svg>
        </div>
        <div>
          <h1 className="header-title">Cattle Tracker</h1>
          <span className="header-subtitle">Smart Farming System v2.4</span>
        </div>
      </div>
      <div className="header-right">
        <div className="cattle-selector">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="12" cy="12" r="10"/><path d="M8 12h8M12 8v8"/>
          </svg>
          <select value={activeCattle} onChange={e => onCattleChange(e.target.value)}>
            {cattleList.map(c => (
              <option key={c.id} value={c.id}>🐄 {c.id} – {c.name}</option>
            ))}
          </select>
        </div>
        <div className={`status-badge ${isOnline ? '' : 'offline'}`}>
          <span className={`status-dot ${isOnline ? 'online' : 'offline'}`}></span>
          <span>{isOnline ? 'Online' : 'Offline'}</span>
        </div>
        <div className="header-time">{time}</div>
      </div>
    </header>
  )
}
