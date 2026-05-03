import { Line } from "react-chartjs-2";
import {
  Chart as ChartJS,
  LineElement,
  PointElement,
  LinearScale,
  TimeScale,
  Tooltip,
  Legend,
} from "chart.js";
import "chartjs-adapter-date-fns";

ChartJS.register(LineElement, PointElement, LinearScale, TimeScale, Tooltip, Legend);

function SensorChart({ history }) {
  if (!history || history.length === 0) {
    return (
      <div className="chart-placeholder">
        <p>No data available for chart</p>
      </div>
    );
  }

  // Filter temperature readings and humidity readings separately
  const tempData = history
    .filter((r) => r.temperature !== null)
    .map((r) => ({ x: new Date(r.recorded_at), y: r.temperature }));

  const humidData = history
    .filter((r) => r.humidity !== null)
    .map((r) => ({ x: new Date(r.recorded_at), y: r.humidity }));

  const chartData = {
    datasets: [
      {
        label: "Temperature (°C)",
        data: tempData,
        borderColor: "#f44336",
        backgroundColor: "rgba(244, 67, 54, 0.1)",
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.3,
      },
      {
        label: "Humidity (%)",
        data: humidData,
        borderColor: "#2196f3",
        backgroundColor: "rgba(33, 150, 243, 0.1)",
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.3,
      },
    ],
  };

  const options = {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      x: {
        type: "time",
        time: {
          unit: "day",
          displayFormats: {
            day: "MMM dd",
          },
        },
        title: {
          display: true,
          text: "Date",
        },
      },
      y: {
        title: {
          display: true,
          text: "Value",
        },
      },
    },
    plugins: {
      legend: {
        display: true,
        position: "top",
      },
      tooltip: {
        mode: "nearest",
        intersect: false,
      },
    },
  };

  return (
    <div className="chart-container">
      <Line data={chartData} options={options} />
    </div>
  );
}

export default SensorChart;