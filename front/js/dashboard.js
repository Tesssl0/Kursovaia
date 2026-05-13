// dashboard.js

import { getAllFlights } from './flights.js';
import { getStatusText, formatDateTime } from './utils.js';

export function updateDashboard() {
    const allFlights = getAllFlights();
    const total = allFlights.length;
    
    const today = allFlights.filter(f => {
        const date = new Date(f.departureTime);
        const now = new Date();
        return date.toDateString() === now.toDateString();
    }).length;
    
    const delayed = allFlights.filter(f => f.status === 'delayed').length;
    const completed = allFlights.filter(f => f.status === 'arrived').length;
    
    const totalEl = document.getElementById('total-flights');
    const todayEl = document.getElementById('today-flights');
    const delayedEl = document.getElementById('delayed-flights');
    const completedEl = document.getElementById('completed-flights');
    
    if (totalEl) totalEl.textContent = total;
    if (todayEl) todayEl.textContent = today;
    if (delayedEl) delayedEl.textContent = delayed;
    if (completedEl) completedEl.textContent = completed;
    
    const upcoming = allFlights
        .filter(f => f.status === 'scheduled')
        .sort((a, b) => new Date(a.departureTime) - new Date(b.departureTime))
        .slice(0, 5);
    
    const container = document.getElementById('upcoming-flights');
    if (!container) return;
    
    container.innerHTML = '';
    
    upcoming.forEach(f => {
        const time = formatDateTime(f.departureTime, 'time');
        const div = document.createElement('div');
        div.className = 'flight-item';
        div.innerHTML = `
            <div class="flight-info">
                <span class="flight-number">${f.number}</span>
                <span class="flight-route">${f.origin} → ${f.destination}</span>
                <span class="flight-time">${time}</span>
            </div>
            <span class="flight-status status-${f.status}">${getStatusText(f.status)}</span>
        `;
        container.appendChild(div);
    });
}