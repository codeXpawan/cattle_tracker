'use client'
import { useRef, useEffect, useMemo } from 'react'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Filler,
  Tooltip,
} from 'chart.js'
import annotationPlugin from 'chartjs-plugin-annotation'
import { Line } from 'react-chartjs-2'

ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Filler, Tooltip, annotationPlugin)

export default function TempChart({ history }) {
  const labels = useMemo(() => history.map(d => d.time), [history])
  const temps = useMemo(() => history.map(d => d.temp), [history])

  const data = {
    labels,
    datasets: [{
      label: 'Temperature (°C)',
      data: temps,
      borderColor: '#448aff',
      backgroundColor: (ctx) => {
        const chart = ctx.chart
        const { ctx: c, chartArea } = chart
        if (!chartArea) return 'rgba(68, 138, 255, 0.1)'
        const gradient = c.createLinearGradient(0, chartArea.top, 0, chartArea.bottom)
        gradient.addColorStop(0, 'rgba(68, 138, 255, 0.25)')
        gradient.addColorStop(1, 'rgba(68, 138, 255, 0.0)')
        return gradient
      },
      borderWidth: 2.5,
      fill: true,
      tension: 0.45,
      pointRadius: 0,
      pointHoverRadius: 6,
      pointHoverBackgroundColor: '#448aff',
      pointHoverBorderColor: '#fff',
      pointHoverBorderWidth: 2,
    }],
  }

  const options = {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 0 },
    interaction: { intersect: false, mode: 'index' },
    scales: {
      x: {
        grid: { color: 'rgba(255,255,255,0.03)', drawBorder: false },
        ticks: { color: '#5a6478', font: { family: 'Inter', size: 10 }, maxRotation: 0, maxTicksLimit: 8 },
        title: { display: true, text: 'Time', color: '#5a6478', font: { family: 'Inter', size: 11, weight: '600' } },
      },
      y: {
        min: 20, max: 41,
        grid: { color: 'rgba(255,255,255,0.03)', drawBorder: false },
        ticks: { color: '#5a6478', font: { family: 'Inter', size: 10 }, callback: v => v + '°C', stepSize: 0.5 },
        title: { display: true, text: 'Temperature (°C)', color: '#5a6478', font: { family: 'Inter', size: 11, weight: '600' } },
      },
    },
    plugins: {
      legend: { display: false },
      tooltip: {
        backgroundColor: 'rgba(15, 22, 36, 0.95)',
        titleColor: '#e8ecf4',
        bodyColor: '#8892a4',
        borderColor: 'rgba(255,255,255,0.08)',
        borderWidth: 1,
        cornerRadius: 10,
        padding: 12,
        callbacks: { label: ctx => `  ${ctx.parsed.y.toFixed(1)}°C` },
      },
      annotation: {
        annotations: {
          safeZone: {
            type: 'box', yMin: 27, yMax: 29,
            backgroundColor: 'rgba(0, 230, 118, 0.06)',
            borderColor: 'rgba(0, 230, 118, 0.15)',
            borderWidth: 1, borderDash: [4, 4],
            label: { display: true, content: 'Safe Zone', position: 'start', color: 'rgba(0, 230, 118, 0.5)', font: { size: 9, weight: '600' } },
          },
          dangerZoneLow: {
            type: 'box', yMin: 25, yMax: 27,
            backgroundColor: 'rgba(255, 23, 68, 0.06)',
            borderColor: 'rgba(255, 23, 68, 0.15)',
            borderWidth: 1, borderDash: [4, 4],
            label: { display: true, content: 'Danger Zone (Low)', position: 'start', color: 'rgba(255, 23, 68, 0.5)', font: { size: 9, weight: '600' } },
          },
          dangerZoneHigh: {
            type: 'box', yMin: 29, yMax: 41,
            backgroundColor: 'rgba(255, 23, 68, 0.06)',
            borderColor: 'rgba(255, 23, 68, 0.15)',
            borderWidth: 1, borderDash: [4, 4],
            label: { display: true, content: 'Danger Zone (High)', position: 'start', color: 'rgba(255, 23, 68, 0.5)', font: { size: 9, weight: '600' } },
          },
        },
      },
    },
  }

  return (
    <div className="chart-panel glass-card">
      <div className="panel-header">
        <h2 className="panel-title">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>
          </svg>
          Real-Time Temperature Monitor
        </h2>
        <div className="chart-legend">
          <span className="legend-item safe"><span className="legend-dot"></span>Safe Zone (38–39°C)</span>
          <span className="legend-item danger"><span className="legend-dot"></span>Danger Zone (&gt;39.5°C)</span>
        </div>
      </div>
      <div className="chart-wrapper">
        <Line data={data} options={options} />
      </div>
    </div>
  )
}
