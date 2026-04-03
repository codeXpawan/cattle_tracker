import { useMemo } from 'react'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Filler,
} from 'chart.js'
import { Line } from 'react-chartjs-2'

// Already registered in TempChart, but safe to re-register
ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Filler)

export default function MobilePreview({ tempData, isOnline, statusLevel, aiResult, aiStatus, onRunAi, activeCattle }) {
  const labels = useMemo(() => tempData.history.map(d => d.time), [tempData.history])
  const temps = useMemo(() => tempData.history.map(d => d.temp), [tempData.history])

  const chartData = {
    labels,
    datasets: [{
      data: temps,
      borderColor: '#00e676',
      backgroundColor: (ctx) => {
        const chart = ctx.chart
        const { ctx: c, chartArea } = chart
        if (!chartArea) return 'rgba(0, 230, 118, 0.1)'
        const gradient = c.createLinearGradient(0, chartArea.top, 0, chartArea.bottom)
        gradient.addColorStop(0, 'rgba(0, 230, 118, 0.2)')
        gradient.addColorStop(1, 'rgba(0, 230, 118, 0.0)')
        return gradient
      },
      borderWidth: 1.5,
      fill: true,
      tension: 0.45,
      pointRadius: 0,
    }],
  }

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 0 },
    scales: { x: { display: false }, y: { display: false, min: 37, max: 41 } },
    plugins: { legend: { display: false }, tooltip: { enabled: false } },
  }

  const statusIcon = statusLevel === 'danger'
    ? <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>
    : statusLevel === 'warning'
    ? <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/></svg>
    : <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3"><polyline points="20 6 9 17 4 12"/></svg>

  const statusTitle = statusLevel === 'danger' ? 'Emergency' : statusLevel === 'warning' ? 'Normal' : 'Good'
  const statusDesc = statusLevel === 'danger' ? 'Fever detected!' : statusLevel === 'warning' ? 'Slight variation' : 'Healthy condition'
  const statusColor = statusLevel === 'danger' ? '#ff1744' : statusLevel === 'warning' ? '#ffca28' : '#00e676'

  const riskColor = aiResult
    ? aiResult.level === 'high' ? '#ff1744' : aiResult.level === 'medium' ? '#ffca28' : '#00e676'
    : '#e8ecf4'

  return (
    <aside className="mobile-preview">
      <div className="phone-frame">
        <div className="phone-notch"></div>
        <div className="phone-screen">

          <div className="mobile-header">
            <div className="mobile-logo">
              <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2"/>
                <path d="M12 8v4l3 3"/>
              </svg>
              <span>Cattle Tracker</span>
            </div>
            <div className={`mobile-status-dot ${isOnline ? 'online' : 'offline'}`}></div>
          </div>

          <div className="mobile-cattle-id">
            <span>🐄 {activeCattle} – {tempData.name}</span>
          </div>

          <div className="mobile-chart-card mobile-glass">
            <span className="mobile-card-title">Temperature</span>
            <div className="mobile-chart-wrap">
              <Line data={chartData} options={chartOptions} />
            </div>
          </div>

          <div className="mobile-temp-display mobile-glass">
            <div className="mobile-temp-value">{tempData.current?.toFixed(1) || '—'}°C</div>
            <div className="mobile-temp-label">Current Temperature</div>
          </div>

          <div className="mobile-status-card mobile-glass">
            <div className={`mobile-status-indicator ${statusLevel}`}>
              {statusIcon}
            </div>
            <div className="mobile-status-info">
              <span className="mobile-status-title" style={{ color: statusColor }}>{statusTitle}</span>
              <span className="mobile-status-desc">{statusDesc}</span>
            </div>
          </div>

          <button
            className={`mobile-ai-btn ${aiStatus === 'running' ? 'running' : ''}`}
            onClick={onRunAi}
            disabled={aiStatus === 'running'}
          >
            {aiStatus === 'running' ? (
              <><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><circle cx="12" cy="12" r="10"/></svg> Analyzing...</>
            ) : (
              <><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><polygon points="5 3 19 12 5 21 5 3"/></svg> Run AI Model</>
            )}
          </button>

          <div className="mobile-ai-result mobile-glass">
            <div className="mobile-ai-row">
              <span>Risk</span>
              <span style={{ color: riskColor }}>{aiResult ? `${aiResult.risk}%` : '—'}</span>
            </div>
            <div className="mobile-ai-row">
              <span>Confidence</span>
              <span>{aiResult ? `${aiResult.confidence}%` : '—'}</span>
            </div>
            <p className="mobile-ai-explanation">
              {aiResult ? aiResult.explanation.slice(0, 80) + '...' : 'Tap Run AI Model to analyze'}
            </p>
          </div>

          <div className="mobile-bottom-nav">
            <div className="nav-item active">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/></svg>
              <span>Home</span>
            </div>
            <div className="nav-item">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>
              <span>Analytics</span>
            </div>
            <div className="nav-item">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.73 21a2 2 0 0 1-3.46 0"/></svg>
              <span>Alerts</span>
            </div>
            <div className="nav-item">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
              <span>Settings</span>
            </div>
          </div>
        </div>
      </div>
    </aside>
  )
}
