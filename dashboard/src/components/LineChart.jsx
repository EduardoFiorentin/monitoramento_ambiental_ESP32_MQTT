// src/components/LineChart.jsx
import React from 'react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from 'chart.js';
import { Line } from 'react-chartjs-2';

ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend);

export default function LineChart({ title, label, color, labels, data }) {
  const options = {
    responsive: true,
    maintainAspectRatio: false,
    animation: {
      duration: 400,
    },
    plugins: {
      legend: { position: 'top' },
      title: { display: true, text: title },
    },
    scales: {
      x: { title: { display: true, text: 'Horário' } },
      y: { title: { display: true, text: 'Valor' } }
    }
  };

  const chartData = {
    labels: labels, // eixo x (horários)
    datasets: [
      {
        label: label,
        data: data, // eixo y (valores)
        borderColor: color,
        backgroundColor: color,
        tension: 0.3,
        pointRadius: 2,
      },
    ],
  };

  return <Line options={options} data={chartData} />;
}