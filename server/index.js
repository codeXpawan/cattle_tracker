import express from 'express';
import cors from 'cors';
import cattleRoutes from './routes/cattle.js';

const app = express();
const PORT = 5005;

app.use(cors());
app.use(express.json());

// API routes
app.use('/api/cattle', cattleRoutes);

app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', uptime: process.uptime() });
});

app.listen(PORT, () => {
  console.log(`🐄 Cattle Tracker API running on http://localhost:${PORT}`);
});
