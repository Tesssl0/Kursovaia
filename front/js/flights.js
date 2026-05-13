// flights.js

import { flightsAPI } from './api.js';
import { showNotification, openEditModal, closeEditModal, showSection } from './ui.js';
import { getAircraftName, getStatusText, formatDateTime, formatForSQL } from './utils.js';
import { isStaff } from './auth.js';

let allFlights = [];
let currentFlights = [];
let currentPage = 1;
const flightsPerPage = 10;

// Загрузка всех рейсов
export async function loadFlights() {
    try {
        allFlights = await flightsAPI.getAll();
        currentFlights = [...allFlights];
        currentPage = 1;
        return allFlights;
    } catch (error) {
        showNotification('Ошибка загрузки рейсов: ' + error.message, 'error');
        return [];
    }
}

// Получить все рейсы
export function getAllFlights() {
    return allFlights;
}

// Загрузка таблицы рейсов
export function loadFlightsTable() {
    const tbody = document.getElementById('flights-table-body');
    if (!tbody) return;
    
    tbody.innerHTML = '';
    
    if (!currentFlights.length) {
        tbody.innerHTML = '<tr><td colspan="11">Нет рейсов для отображения</td></tr>';
        return;
    }
    
    const start = (currentPage - 1) * flightsPerPage;
    const pageFlights = currentFlights.slice(start, start + flightsPerPage);
    const isStaffUser = isStaff();
    
    pageFlights.forEach(f => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td>${f.id}</td>
            <td><strong>${f.number}</strong></td>
            <td>${f.origin} → ${f.destination}</td>
            <td>${formatDateTime(f.departureTime)}</td>
            <td>${formatDateTime(f.arrivalTime)}</td>
            <td>${getAircraftName(f.aircraft)}</td>
            <td>${f.seats}</td>
            <td><span class="flight-status status-${f.status}">${getStatusText(f.status)}</span></td>
            <td>${f.terminal || "—"}</td>
            <td>${f.gate || "—"}</td>
            <td>
                ${isStaffUser ? `
                    <button class="btn-success btn-small" onclick="window.changeFlightStatusHandler(${f.id}, 'departed')" title="Вылетел">✈️</button>
                    <button class="btn-warning btn-small" onclick="window.changeFlightStatusHandler(${f.id}, 'delayed')" title="Задержан">⏰</button>
                    <button class="btn-danger btn-small" onclick="window.deleteFlightHandler(${f.id})" title="Удалить">🗑️</button>
                    <button class="btn-info btn-small" onclick="window.editFlightHandler(${f.id})" title="Редактировать">✏️</button>
                ` : ""}
            </td>
        `;
        tbody.appendChild(row);
    });
    
    updatePaginationInfo();
}

function updatePaginationInfo() {
    const totalPages = Math.ceil(currentFlights.length / flightsPerPage);
    const pageInfo = document.getElementById('page-info');
    if (pageInfo) {
        pageInfo.textContent = `Страница ${currentPage} из ${totalPages || 1}`;
    }
}

export function applyFlightFilters() {
    const searchTerm = document.getElementById('search-flight')?.value.trim().toLowerCase() || '';
    const status = document.getElementById('filter-status')?.value || 'all';
    
    currentFlights = allFlights.filter(flight => {
        const matchesNumber = flight.number.toLowerCase().includes(searchTerm);
        const matchesStatus = status === "all" || flight.status === status;
        return matchesNumber && matchesStatus;
    });
    
    currentPage = 1;
    loadFlightsTable();
}

export function refreshFlights() {
    const searchInput = document.getElementById('search-flight');
    const statusSelect = document.getElementById('filter-status');
    
    if (searchInput) searchInput.value = "";
    if (statusSelect) statusSelect.value = "all";
    
    currentFlights = [...allFlights];
    currentPage = 1;
    loadFlightsTable();
    showNotification("Список рейсов обновлен");
}

export async function addFlight(event) {
    event.preventDefault();
    
    const flightData = {
        number: document.getElementById("flight-number").value.trim(),
        origin: document.getElementById("departure-airport").value,
        destination: document.getElementById("arrival-airport").value,
        departureTime: formatForSQL(document.getElementById("departure-time").value),
        arrivalTime: formatForSQL(document.getElementById("arrival-time").value),
        aircraft: parseInt(document.getElementById("aircraft").value),
        status: document.getElementById("flight-status").value,
        seats: parseInt(document.getElementById("seats").value),
        terminal: document.getElementById("flight-terminal").value,
        gate: document.getElementById("flight-gate").value
    };
    
    if (flightData.origin === flightData.destination) {
        showNotification("Аэропорт отправления и прибытия не могут совпадать", "error");
        return;
    }
    
    try {
        await flightsAPI.create(flightData);
        await loadFlights();
        loadFlightsTable();
        showNotification("Рейс добавлен");
        document.getElementById("flight-form")?.reset();
        showSection('flights');
    } catch (error) {
        showNotification("Ошибка при добавлении рейса: " + error.message, "error");
    }
}

export async function updateFlightStatus(id, newStatus) {
    try {
        await flightsAPI.updateStatus(id, newStatus);
        await loadFlights();
        loadFlightsTable();
        showNotification(`Статус обновлён: ${getStatusText(newStatus)}`);
    } catch (error) {
        showNotification("Ошибка при обновлении статуса", "error");
    }
}

export async function deleteFlight(id) {
    if (!confirm('Вы уверены, что хотите удалить этот рейс?')) return;
    
    try {
        await flightsAPI.delete(id);
        await loadFlights();
        loadFlightsTable();
        showNotification("Рейс удалён");
    } catch (error) {
        showNotification("Ошибка при удалении рейса", "error");
    }
}

export async function handleEditSubmit(event) {
    event.preventDefault();
    
    const id = document.getElementById("edit-id").value;
    
    const flightData = {
        number: document.getElementById("edit-number").value.trim(),
        origin: document.getElementById("edit-origin").value,
        destination: document.getElementById("edit-destination").value,
        departureTime: formatForSQL(document.getElementById("edit-departure").value),
        arrivalTime: formatForSQL(document.getElementById("edit-arrival").value),
        aircraft: parseInt(document.getElementById("edit-aircraft").value),
        seats: parseInt(document.getElementById("edit-seats").value),
        status: document.getElementById("edit-status").value,
        terminal: document.getElementById("edit-terminal").value,
        gate: document.getElementById("edit-gate").value
    };
    
    if (flightData.origin === flightData.destination) {
        showNotification("Аэропорт отправления и прибытия не могут совпадать", "error");
        return;
    }
    
    try {
        await flightsAPI.update(id, flightData);
        await loadFlights();
        loadFlightsTable();
        closeEditModal();
        showNotification("Изменения сохранены");
    } catch (error) {
        showNotification("Ошибка при обновлении рейса", "error");
    }
}

export function nextPage() {
    const totalPages = Math.ceil(currentFlights.length / flightsPerPage);
    if (currentPage < totalPages) {
        currentPage++;
        loadFlightsTable();
    }
}

export function prevPage() {
    if (currentPage > 1) {
        currentPage--;
        loadFlightsTable();
    }
}

export async function importFlightsFromFile(fileContent) {
    try {
        const result = await flightsAPI.import(fileContent);
        showNotification(`Импорт завершен: ${result.success} добавлено, ${result.errors} ошибок`);
        
        if (result.errorList && result.errorList.length > 0) {
            console.error("Ошибки импорта:", result.errorList);
            // Показать первые 5 ошибок
            const errorMsg = result.errorList.slice(0, 5).join('\n');
            if (result.errorList.length > 5) {
                showNotification(`Ошибки:\n${errorMsg}\n... и еще ${result.errorList.length - 5}`, "error");
            } else {
                showNotification(`Ошибки:\n${errorMsg}`, "error");
            }
        }
        
        await loadFlights();
        loadFlightsTable();
        return result;
    } catch (error) {
        showNotification("Ошибка импорта: " + error.message, "error");
        throw error;
    }
}

// flights.js - добавить функцию

// Экспорт рейсов в TXT файл
export function exportFlightsToTXT() {
    if (!allFlights.length) {
        showNotification("Нет рейсов для экспорта", "error");
        return;
    }
    
    // Формируем содержимое файла (формат: номер,откуда,куда,время_вылета,время_прилета,самолет,статус,мест,терминал,gate)
    let content = "";
    allFlights.forEach(flight => {
        // Преобразуем время в нужный формат (без секунд)
        let departure = flight.departureTime.replace(" ", "T").slice(0, 16);
        let arrival = flight.arrivalTime.replace(" ", "T").slice(0, 16);
        
        // Формируем строку
        const line = [
            flight.number,
            flight.origin,
            flight.destination,
            departure,
            arrival,
            flight.aircraft,
            flight.status,
            flight.seats,
            flight.terminal || "",
            flight.gate || ""
        ].join(",");
        
        content += line + "\n";
    });
    
    // Создаем и скачиваем файл
    const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    
    // Имя файла с текущей датой
    const now = new Date();
    const dateStr = `${now.getFullYear()}-${now.getMonth()+1}-${now.getDate()}`;
    a.download = `рейсы_${dateStr}.txt`;
    
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    showNotification(`Экспортировано ${allFlights.length} рейсов`);
}

// Для экспорта ТОЛЬКО отфильтрованных рейсов (текущие фильтры)
export function exportFilteredFlightsToTXT() {
    if (!currentFlights.length) {
        showNotification("Нет рейсов для экспорта", "error");
        return;
    }
    
    let content = "";
    currentFlights.forEach(flight => {
        let departure = flight.departureTime.replace(" ", "T").slice(0, 16);
        let arrival = flight.arrivalTime.replace(" ", "T").slice(0, 16);
        
        const line = [
            flight.number,
            flight.origin,
            flight.destination,
            departure,
            arrival,
            flight.aircraft,
            flight.status,
            flight.seats,
            flight.terminal || "",
            flight.gate || ""
        ].join(",");
        
        content += line + "\n";
    });
    
    const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    
    const now = new Date();
    const dateStr = `${now.getFullYear()}-${now.getMonth()+1}-${now.getDate()}`;
    a.download = `рейсы_фильтр_${dateStr}.txt`;
    
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    showNotification(`Экспортировано ${currentFlights.length} рейсов (по текущему фильтру)`);
}

// Глобальный обработчик для файла
window.handleFileImport = async (event) => {
    const file = event.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = async (e) => {
        const content = e.target.result;
        await importFlightsFromFile(content);
        event.target.value = ''; // Очистить input
    };
    reader.readAsText(file, 'UTF-8');
};