export default function StatsRow({ current, heartRate, activity, uptime }) {
  const activityColor = activity === 'Restless' ? '#ff1744' : activity === 'Active' ? '#ffca28' : '#00e676'

  return (
    <section className="stats-row">
      <div className="stat-card">
        <div className="stat-icon temp-icon">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/>
          </svg>
        </div>
        <div className="stat-info">
          <span className="stat-label">Current Temp</span>
          <span className="stat-value">{current?.toFixed(1) || '—'}°C</span>
        </div>
      </div>

      <div className="stat-card">
        <div className="stat-icon heart-icon">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78z"/>
          </svg>
        </div>
        <div className="stat-info">
          <span className="stat-label">Heart Rate</span>
          <span className="stat-value">{heartRate || '—'} bpm</span>
        </div>
      </div>

      <div className="stat-card">
        <div className="stat-icon activity-icon">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>
          </svg>
        </div>
        <div className="stat-info">
          <span className="stat-label">Activity</span>
          <span className="stat-value" style={{ color: activityColor }}>{activity}</span>
        </div>
      </div>

      <div className="stat-card">
        <div className="stat-icon uptime-icon">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>
          </svg>
        </div>
        <div className="stat-info">
          <span className="stat-label">Uptime</span>
          <span className="stat-value">{uptime}%</span>
        </div>
      </div>
    </section>
  )
}
