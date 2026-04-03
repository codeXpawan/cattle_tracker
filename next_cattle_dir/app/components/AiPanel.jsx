'use client'
export default function AiPanel({ aiStatus, aiResult, onRun, cattleName }) {
  const dotClass = aiStatus === 'running' ? 'running' : aiStatus === 'completed' ? 'completed' : 'idle'
  const statusLabel = aiStatus === 'running' ? 'Running' : aiStatus === 'completed' ? 'Completed' : 'Idle'

  const riskClass = aiResult
    ? aiResult.level === 'high' ? 'risk-high' : aiResult.level === 'medium' ? 'risk-medium' : 'risk-low'
    : ''

  return (
    <div className="ai-panel glass-card">
      <div className="panel-header">
        <h2 className="panel-title">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M12 2a4 4 0 0 1 4 4v2a4 4 0 0 1-8 0V6a4 4 0 0 1 4-4z"/>
            <path d="M16 14v1a4 4 0 0 1-8 0v-1"/>
            <line x1="12" y1="18" x2="12" y2="22"/>
            <line x1="8" y1="22" x2="16" y2="22"/>
          </svg>
          AI Health Predictor
        </h2>
      </div>

      <div className="ai-status-row">
        <span className="ai-status-label">Model Status</span>
        <span className="ai-status-badge">
          <span className={`ai-status-dot ${dotClass}`}></span>
          <span>{statusLabel}</span>
        </span>
      </div>

      <button className={`ai-run-btn ${aiStatus === 'running' ? 'running' : ''}`} onClick={onRun} disabled={aiStatus === 'running'}>
        {aiStatus === 'running' ? (
          <>
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><circle cx="12" cy="12" r="10"/></svg>
            Analyzing...
          </>
        ) : (
          <>
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><polygon points="5 3 19 12 5 21 5 3"/></svg>
            Run AI Model
          </>
        )}
      </button>

      <div className="ai-results">
        <div className="ai-result-item">
          <span className="ai-result-label">Health Risk</span>
          <span className={`ai-result-value ${riskClass}`}>{aiResult ? `${aiResult.risk}%` : '—'}</span>
        </div>
        <div className="ai-result-item">
          <span className="ai-result-label">Confidence</span>
          <span className="ai-result-value">{aiResult ? `${aiResult.confidence}%` : '—'}</span>
        </div>
        <div className="ai-result-item explanation">
          <span className="ai-result-label">Analysis</span>
          <p className="ai-explanation">
            {aiResult ? aiResult.explanation : `Run the AI model to generate a health prediction for ${cattleName || 'the selected cattle'}.`}
          </p>
        </div>
      </div>

      <div className="ai-model-info">
        <span>Model: CattleHealth-LSTM v3.2</span>
        <span>Last trained: 2 hours ago</span>
      </div>
    </div>
  )
}
