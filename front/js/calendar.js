// calendar.js

import { getAllFlights } from './flights.js';
import { getAircraftName, formatDateTime, getMonday } from './utils.js';
import { openEditModal } from './ui.js';

let currentWeekStart = getMonday(new Date());

// Загрузка календаря
export function loadCalendar() {
    const calendarBody = document.getElementById("calendar-body");
    if (!calendarBody) return;
    
    calendarBody.innerHTML = "";
    
    const allFlights = getAllFlights();
    const weekDays = [];
    
    for (let i = 0; i < 7; i++) {
        const day = new Date(currentWeekStart);
        day.setDate(day.getDate() + i);
        weekDays.push(day);
    }
    
    weekDays.forEach(day => {
        const dayDiv = document.createElement("div");
        dayDiv.className = "calendar-day";
        
        const dayNumber = day.getDate();
        const month = day.toLocaleString("ru-RU", { month: "short" });
        
        dayDiv.innerHTML = `
            <div class="day-number">${dayNumber} ${month}</div>
            <div class="day-flights"></div>
        `;
        
        const flightsForDay = allFlights.filter(f => {
            const dep = new Date(f.departureTime);
            return dep.toDateString() === day.toDateString();
        });
        
        const flightsContainer = dayDiv.querySelector(".day-flights");
        
        flightsForDay.forEach(f => {
            const time = formatDateTime(f.departureTime, 'time');
            
            const flightDiv = document.createElement("div");
            flightDiv.className = `calendar-flight status-${f.status}`;
            flightDiv.innerHTML = `
                <div class="flight-header">
                    <span class="flight-time">🕓 ${time}</span>
                    <span class="flight-number">№ ${f.number}</span>
                </div>
                <div class="flight-route">🛫 ${f.origin} → ${f.destination}</div>
                <div class="flight-aircraft">✈️ ${getAircraftName(f.aircraft)}</div>
                <div class="flight-extra">
                    <span class="flight-terminal">🏢 Терминал: ${f.terminal || "—"}</span>
                    <span class="flight-gate">🏁 Gate: ${f.gate || "—"}</span>
                </div>
            `;
            
            flightDiv.addEventListener("click", () => openEditModal(f));
            flightsContainer.appendChild(flightDiv);
        });
        
        calendarBody.appendChild(dayDiv);
    });
    
    updateWeekHeader();
}

// Обновление заголовка недели
function updateWeekHeader() {
    const header = document.getElementById("current-week");
    if (!header) return;
    
    const start = currentWeekStart;
    const end = new Date(start);
    end.setDate(end.getDate() + 6);
    
    const startStr = start.toLocaleDateString("ru-RU", { day: "numeric", month: "long" });
    const endStr = end.toLocaleDateString("ru-RU", { day: "numeric", month: "long" });
    
    header.textContent = `${startStr} — ${endStr}`;
}

// Навигация по неделям
export function prevWeek() {
    currentWeekStart.setDate(currentWeekStart.getDate() - 7);
    loadCalendar();
}

export function nextWeek() {
    currentWeekStart.setDate(currentWeekStart.getDate() + 7);
    loadCalendar();
}

export function resetWeek() {
    currentWeekStart = getMonday(new Date());
    loadCalendar();
}