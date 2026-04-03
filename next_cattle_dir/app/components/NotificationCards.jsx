'use client'
export default function NotificationCards({ statusLevel }) {
  return (
    <section className="notifications">
      <h2 className="section-title">
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
          <path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/>
          <path d="M13.73 21a2 2 0 0 1-3.46 0"/>
        </svg>
        Health Notifications
      </h2>
      <div className="notif-grid">
        <div className={`notif-card notif-good ${statusLevel === 'good' ? 'active-notif' : ''}`}>
          <div className="notif-icon-wrap good">
            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
              <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/>
              <polyline points="22 4 12 14.01 9 11.01"/>
            </svg>
          </div>
          <div className="notif-content">
            <span className="notif-title">Good – Healthy Condition</span>
            <span className="notif-desc">Temperature within normal range (38–39°C). No immediate action required.</span>
          </div>
          <span className="notif-time">{statusLevel === 'good' ? 'Just now' : '—'}</span>
        </div>

        <div className={`notif-card notif-warning ${statusLevel === 'warning' ? 'active-notif' : ''}`}>
          <div className="notif-icon-wrap warning">
            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
              <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/>
              <line x1="12" y1="9" x2="12" y2="13"/>
              <line x1="12" y1="17" x2="12.01" y2="17"/>
            </svg>
          </div>
          <div className="notif-content">
            <span className="notif-title">Normal – Slight Variation</span>
            <span className="notif-desc">Minor temperature fluctuation detected (39–39.5°C). Monitor closely.</span>
          </div>
          <span className="notif-time">{statusLevel === 'warning' ? 'Just now' : '5 min ago'}</span>
        </div>

        <div className={`notif-card notif-danger ${statusLevel === 'danger' ? 'active-notif' : ''}`}>
          <div className="notif-icon-wrap danger">
            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
              <circle cx="12" cy="12" r="10"/>
              <line x1="15" y1="9" x2="9" y2="15"/>
              <line x1="9" y1="9" x2="15" y2="15"/>
            </svg>
          </div>
          <div className="notif-content">
            <span className="notif-title">Emergency – Fever Detected</span>
            <span className="notif-desc">Temperature exceeded 39.5°C. Immediate veterinary attention recommended.</span>
          </div>
          <span className="notif-time">{statusLevel === 'danger' ? 'Just now' : '12 min ago'}</span>
        </div>
      </div>
    </section>
  )
}
