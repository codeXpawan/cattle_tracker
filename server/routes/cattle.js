import { Router } from 'express';

const router = Router();

// ── Cattle Profiles ──────────────────────────────────────────
const cattleProfiles = {
  'CTL-001': { id: 'CTL-001', name: 'Bella',  baseTemp: 38.5, variance: 0.3, heartBase: 70 },
  'CTL-002': { id: 'CTL-002', name: 'Daisy',  baseTemp: 38.8, variance: 0.4, heartBase: 68 },
  'CTL-003': { id: 'CTL-003', name: 'Rosie',  baseTemp: 38.3, variance: 0.25, heartBase: 72 },
  'CTL-004': { id: 'CTL-004', name: 'Clover', baseTemp: 39.0, variance: 0.5, heartBase: 66 },
};

// In-memory temperature history per cattle
const tempHistory = {};
const MAX_HISTORY = 30;

function generateTemp(profile) {
  const random = (Math.random() - 0.5) * 2 * profile.variance;
  const spike = Math.random() > 0.92 ? Math.random() * 1.5 : 0;
  return Math.round((profile.baseTemp + random + spike) * 10) / 10;
}

function formatTime(date) {
  return date.toLocaleTimeString('en-US', {
    hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false,
  });
}

// Seed initial data for all cattle
for (const [id, profile] of Object.entries(cattleProfiles)) {
  tempHistory[id] = [];
  const now = Date.now();
  for (let i = MAX_HISTORY - 1; i >= 0; i--) {
    tempHistory[id].push({
      time: formatTime(new Date(now - i * 2000)),
      temp: generateTemp(profile),
    });
  }
}

// Push new readings every 2 seconds
setInterval(() => {
  const now = new Date();
  for (const [id, profile] of Object.entries(cattleProfiles)) {
    tempHistory[id].push({
      time: formatTime(now),
      temp: generateTemp(profile),
    });
    if (tempHistory[id].length > MAX_HISTORY) {
      tempHistory[id].shift();
    }
  }
  // Notify SSE clients
  for (const client of sseClients) {
    const data = tempHistory[client.cattleId];
    if (data) {
      client.res.write(`data: ${JSON.stringify(data[data.length - 1])}\n\n`);
    }
  }
}, 2000);

// SSE clients
const sseClients = [];

// ── Routes ───────────────────────────────────────────────────

// GET /api/cattle — list all cattle
router.get('/', (req, res) => {
  const list = Object.values(cattleProfiles).map((p) => ({
    id: p.id, name: p.name,
  }));
  res.json(list);
});

// GET /api/cattle/:id/temperature — get temp history
router.get('/:id/temperature', (req, res) => {
  const { id } = req.params;
  if (!tempHistory[id]) return res.status(404).json({ error: 'Cattle not found' });
  const profile = cattleProfiles[id];
  const latest = tempHistory[id][tempHistory[id].length - 1];
  const hr = Math.round(profile.heartBase + (latest.temp - 37.5) * 8 + (Math.random() - 0.5) * 4);
  let activity = 'Normal';
  if (latest.temp > 39.5) activity = 'Restless';
  else if (latest.temp > 39) activity = 'Active';

  res.json({
    cattleId: id,
    name: profile.name,
    history: tempHistory[id],
    current: latest.temp,
    heartRate: hr,
    activity,
    uptime: 99.8,
  });
});

// GET /api/cattle/:id/stream — SSE
router.get('/:id/stream', (req, res) => {
  const { id } = req.params;
  if (!cattleProfiles[id]) return res.status(404).json({ error: 'Cattle not found' });

  res.writeHead(200, {
    'Content-Type': 'text/event-stream',
    'Cache-Control': 'no-cache',
    Connection: 'keep-alive',
  });

  const client = { cattleId: id, res };
  sseClients.push(client);

  req.on('close', () => {
    const idx = sseClients.indexOf(client);
    if (idx > -1) sseClients.splice(idx, 1);
  });
});

// POST /api/cattle/:id/ai-predict — AI model simulation
router.post('/:id/ai-predict', (req, res) => {
  const { id } = req.params;
  if (!tempHistory[id]) return res.status(404).json({ error: 'Cattle not found' });

  const profile = cattleProfiles[id];
  const recent = tempHistory[id].slice(-10);
  const avgTemp = recent.reduce((s, d) => s + d.temp, 0) / recent.length;

  // Simulate 2-second processing delay
  setTimeout(() => {
    let risk, confidence, explanation, level;

    if (avgTemp >= 39.5) {
      risk = Math.round(75 + Math.random() * 20);
      confidence = Math.round(88 + Math.random() * 10);
      explanation = `High fever pattern detected. Average temperature of ${avgTemp.toFixed(1)}°C over the last 20 seconds exceeds the danger threshold. Recommend immediate veterinary check-up and isolation for ${profile.name}.`;
      level = 'high';
    } else if (avgTemp >= 39.0) {
      risk = Math.round(30 + Math.random() * 25);
      confidence = Math.round(82 + Math.random() * 12);
      explanation = `Mild elevation detected. ${profile.name}'s temperature averaging ${avgTemp.toFixed(1)}°C indicates a slight deviation from baseline. Continue monitoring; no immediate intervention needed.`;
      level = 'medium';
    } else {
      risk = Math.round(5 + Math.random() * 15);
      confidence = Math.round(90 + Math.random() * 9);
      explanation = `${profile.name} is in excellent health. Temperature averaging ${avgTemp.toFixed(1)}°C is well within the safe range. All vitals are stable. No action required.`;
      level = 'low';
    }

    res.json({ risk, confidence, explanation, level, avgTemp: avgTemp.toFixed(1) });
  }, 2000);
});

export default router;
