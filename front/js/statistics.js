// statistics.js

import { statsAPI } from './api.js';
import { getAllFlights } from './flights.js';
import { formatDateTime } from './utils.js';

let statusChart = null;
let terminalChart = null;

// Загрузка всей статистики
export async function loadStatistics() {
    await loadFlightStatuses();
    await loadTerminalStats();
    await loadUpcomingFlights();
}

// Загрузка статусов рейсов
async function loadFlightStatuses() {
    const stats = {
        scheduled: 0,
        delayed: 0,
        departed: 0,
        arrived: 0,
        cancelled: 0
    };
    
    const allFlights = getAllFlights();
    allFlights.forEach(f => {
        if (stats[f.status] !== undefined) {
            stats[f.status]++;
        }
    });
    
    const ctx = document.getElementById("chartStatuses");
    if (!ctx) return;
    
    // Уничтожаем старый график
    if (statusChart) statusChart.destroy();
    
    statusChart = new Chart(ctx, {
        type: "pie",
        data: {
            labels: Object.keys(stats).map(s => getStatusLabel(s)),
            datasets: [{
                data: Object.values(stats),
                backgroundColor: ["#4caf50", "#ff9800", "#2196f3", "#9c27b0", "#f44336"]
            }]
        },
        options: {
            responsive: true,
            plugins: {
                legend: { position: 'bottom' }
            }
        }
    });
}

// Загрузка статистики по терминалам
async function loadTerminalStats() {
    const terminals = {};
    
    const allFlights = getAllFlights();
    allFlights.forEach(f => {
        const t = f.terminal || "—";
        terminals[t] = (terminals[t] || 0) + 1;
    });
    
    const ctx = document.getElementById("chartTerminals");
    if (!ctx) return;
    
    if (terminalChart) terminalChart.destroy();
    
    terminalChart = new Chart(ctx, {
        type: "bar",
        data: {
            labels: Object.keys(terminals),
            datasets: [{
                label: "Рейсов",
                data: Object.values(terminals),
                backgroundColor: "#2196f3"
            }]
        },
        options: {
            responsive: true,
            scales: { y: { beginAtZero: true } },
            plugins: { legend: { position: 'bottom' } }
        }
    });
}

// Загрузка ближайших рейсов
async function loadUpcomingFlights() {
    const allFlights = getAllFlights();
    const upcoming = allFlights
        .filter(f => f.status === 'scheduled')
        .sort((a, b) => new Date(a.departureTime) - new Date(b.departureTime))
        .slice(0, 5);
    
    const list = document.getElementById("upcomingList");
    if (!list) return;
    
    list.innerHTML = "";
    
    upcoming.forEach(f => {
        const time = formatDateTime(f.departureTime, 'time');
        const li = document.createElement("li");
        li.textContent = `${time} — ${f.number} (${f.origin} → ${f.destination})`;
        list.appendChild(li);
    });
}

// Вспомогательная функция для получения текста статуса
function getStatusLabel(status) {
    const labels = {
        'scheduled': 'По расписанию',
        'delayed': 'Задержан',
        'departed': 'Вылетел',
        'arrived': 'Прибыл',
        'cancelled': 'Отменен'
    };
    return labels[status] || status;
}