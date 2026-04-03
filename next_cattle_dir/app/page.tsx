'use client'
import { useState, useEffect, useCallback, useRef } from 'react'
import Header from './components/Header'
import StatsRow from './components/StatsRow'
import TempChart from './components/TempChart'
import AiPanel from './components/AiPanel'
import NotificationCards from './components/NotificationCards'
import MobilePreview from './components/MobilePreview'

const API = 'http://localhost:5005/api/cattle'

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

  // Fetch temperature data and set up SSE
  useEffect(() => {
    let cancelled = false

    // Initial fetch
    const fetchData = () => {
      fetch(`${API}/${activeCattle}/temperature`)
        .then(r => r.json())
        .then(data => {
          if (!cancelled) {
            setTempData(data)
            setIsOnline(true)
          }
        })
        .catch(() => !cancelled && setIsOnline(false))
    }

    fetchData()

    // SSE for live updates
    if (eventSourceRef.current) {
      eventSourceRef.current.close()
    }

    const es = new EventSource(`${API}/${activeCattle}/stream`)
    eventSourceRef.current = es

    es.onmessage = (event) => {
      const point = JSON.parse(event.data)
      setTempData(prev => {
        const history = [...prev.history, point]
        if (history.length > 30) history.shift()
        const latest = point.temp
        const hr = Math.round(68 + (latest - 37.5) * 8 + (Math.random() - 0.5) * 4)
        let activity = 'Normal'
        if (latest > 39.5) activity = 'Restless'
        else if (latest > 39) activity = 'Active'
        return { ...prev, history, current: latest, heartRate: hr, activity }
      })
      setIsOnline(true)
    }

    es.onerror = () => setIsOnline(false)

    // Reset AI when cattle changes
    setAiResult(null)
    setAiStatus('idle')

    return () => {
      cancelled = true
      es.close()
    }
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
