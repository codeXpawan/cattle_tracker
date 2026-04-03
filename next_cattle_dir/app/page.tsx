'use client'
import { useState, useEffect, useCallback, useRef } from 'react'
import Header from './components/Header'
import StatsRow from './components/StatsRow'
import TempChart from './components/TempChart'
import AiPanel from './components/AiPanel'
import NotificationCards from './components/NotificationCards'
import MobilePreview from './components/MobilePreview'

const API = 'http://192.168.4.1/api/cattle'

interface TempPoint {
  time: string;
  temp: number;
}

interface TempData {
  history: TempPoint[];
  current: number;
  heartRate: number;
  activity: string;
  uptime: number;
  name: string;
}

export default function Home() {
  const [cattleList, setCattleList] = useState<any[]>([])
  const [activeCattle, setActiveCattle] = useState('CTL-001')
  const [tempData, setTempData] = useState<TempData>({ history: [], current: 0, heartRate: 0, activity: 'Normal', uptime: 99.8, name: '' })
  const [aiResult, setAiResult] = useState<any>(null)
  const [aiStatus, setAiStatus] = useState<'idle' | 'running' | 'completed'>('idle')
  const [isOnline, setIsOnline] = useState(true)
  const eventSourceRef = useRef<EventSource | null>(null)

  // Fetch cattle list on mount
  useEffect(() => {
    fetch(API)
      .then(r => r.json())
      .then(setCattleList)
      .catch(() => setIsOnline(false))
  }, [])

  // Push current time to ESP32 on mount
useEffect(() => {
    // Push time to ESP32 once on mount
    const iso = new Date().toISOString().slice(0, 19)
    fetch('http://192.168.4.1/api/cattle/time', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ time: iso })
    }).catch(e => console.warn('Time sync failed:', e))

    // Poll temperature every 5 seconds instead of SSE
    const poll = () => {
        fetch('http://192.168.4.1/api/cattle/CTL-001/temperature')
            .then(r => r.json())
            .then(data => {
                setTempData(data)
                setIsOnline(true)
            })
            .catch(() => setIsOnline(false))
    }

    poll() // immediate first call
    const interval = setInterval(poll, 5000)
    return () => clearInterval(interval)  // cleanup on unmount
}, [activeCattle])

  // Run AI model
  const runAi = useCallback(async () => {
    if (aiStatus === 'running') return
    setAiStatus('running')
    setAiResult(null)
    try {
      const res = await fetch(`${API}/${activeCattle}/ai-predict`, { method: 'POST' })
      const data = await res.json()
      setAiResult(data)
      setAiStatus('completed')
    } catch {
      setAiStatus('idle')
    }
  }, [activeCattle, aiStatus])

  // Get current status level
  const getStatusLevel = () => {
    if (tempData.current >= 39.5) return 'danger'
    if (tempData.current >= 39.0) return 'warning'
    return 'good'
  }

  return (
    <div className="app-container">
      <main className="dashboard">
        <Header
          cattleList={cattleList}
          activeCattle={activeCattle}
          onCattleChange={setActiveCattle}
          isOnline={isOnline}
        />
        <StatsRow
          current={tempData.current}
          heartRate={tempData.heartRate}
          activity={tempData.activity}
          uptime={tempData.uptime}
        />
        <section className="main-content">
          <TempChart history={tempData.history} />
          <AiPanel
            aiStatus={aiStatus}
            aiResult={aiResult}
            onRun={runAi}
            cattleName={tempData.name}
          />
        </section>
        <NotificationCards statusLevel={getStatusLevel()} />
      </main>

      <MobilePreview
        tempData={tempData}
        isOnline={isOnline}
        statusLevel={getStatusLevel()}
        aiResult={aiResult}
        aiStatus={aiStatus}
        onRunAi={runAi}
        activeCattle={activeCattle}
      />
    </div>
  )
}
