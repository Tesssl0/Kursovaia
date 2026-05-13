// app.js - Главный модуль

import { loadFlights, loadFlightsTable, addFlight, handleEditSubmit, 
         updateFlightStatus, deleteFlight, applyFlightFilters, refreshFlights,
         nextPage, prevPage } from './flights.js';
import { loadCalendar, prevWeek, nextWeek, resetWeek } from './calendar.js';
import { loadStatistics } from './statistics.js';
import { login, register, logout, checkAuthState } from './auth.js';
import { showSection, showNotification, openEditModal, closeEditModal, 
         openAuthModal, closeAuthModal, switchAuthTab, updateUserInterface } from './ui.js';
import { updateDashboard } from './dashboard.js';
import { getAllFlights } from './flights.js';
import { getStatusText } from './utils.js';
import { airportsAPI, aircraftAPI } from './api.js';
import { exportFlightsToTXT, exportFilteredFlightsToTXT } from './flights.js';

// Глобальные обработчики для onclick в HTML
window.showSection = showSection;
window.openAuthModal = openAuthModal;
window.closeAuthModal = closeAuthModal;
window.switchAuthTab = switchAuthTab;
window.prevWeek = prevWeek;
window.nextWeek = nextWeek;
window.resetWeek = resetWeek;
window.refreshFlights = refreshFlights;
window.nextPage = nextPage;
window.prevPage = prevPage;
window.applyFlightFilters = applyFlightFilters;
window.openEditModal = openEditModal;
window.closeEditModal = closeEditModal;
window.exportFlightsToTXT = exportFlightsToTXT;
window.exportFilteredFlightsToTXT = exportFilteredFlightsToTXT;

// Обработчики для рейсов
window.changeFlightStatusHandler = updateFlightStatus;
window.deleteFlightHandler = deleteFlight;
window.editFlightHandler = (id) => {
    const flight = getAllFlights().find(f => f.id === id);
    if (flight) openEditModal(flight);
};
window.logoutHandler = logout;
window.addFlight = addFlight;

// Функции авторизации
window.loginHandler = async () => {
    const username = document.getElementById("login-username").value;
    const password = document.getElementById("login-password").value;
    
    const result = await login(username, password);
    if (result.success) {
        closeAuthModal();
        updateUserInterface(result.data.role, result.data.username);
        await initApp();
        showNotification(`Добро пожаловать, ${result.data.username}!`);
    } else {
        showNotification("Ошибка: " + result.error, "error");
    }
};

window.registerHandler = async () => {
    const username = document.getElementById("reg-username").value;
    const password = document.getElementById("reg-password").value;
    const password2 = document.getElementById("reg-password2").value;
    
    const result = await register(username, password, password2);
    if (result.success) {
        showNotification("Регистрация успешна! Теперь войдите в систему");
        switchAuthTab('login');
    } else {
        showNotification("Ошибка: " + result.error, "error");
    }
};

// ==================== НОВЫЕ ФУНКЦИИ ====================

// Загрузка аэропортов
async function loadAirports() {
    try {
        const airports = await airportsAPI.getAll();
        const selectors = ['departure-airport', 'arrival-airport', 'edit-origin', 'edit-destination'];
        
        for (const selectorId of selectors) {
            const select = document.getElementById(selectorId);
            if (!select) continue;
            
            select.innerHTML = '<option value="">Выберите аэропорт</option>';
            airports.forEach(airport => {
                const option = document.createElement('option');
                option.value = airport.code;
                option.textContent = `${airport.name} (${airport.code}), ${airport.city}`;
                select.appendChild(option);
            });
        }
    } catch (error) {
        console.error('Ошибка загрузки аэропортов:', error);
        showNotification('Ошибка загрузки списка аэропортов', 'error');
    }
}

// Загрузка самолётов
async function loadAircraft() {
    try {
        const aircraft = await aircraftAPI.getAll();
        const selectors = ['aircraft', 'edit-aircraft'];
        
        for (const selectorId of selectors) {
            const select = document.getElementById(selectorId);
            if (!select) continue;
            
            select.innerHTML = '<option value="">Выберите самолёт</option>';
            aircraft.forEach(plane => {
                const option = document.createElement('option');
                option.value = plane.id;
                option.textContent = `${plane.manufacturer} ${plane.model} (${plane.seats} мест)`;
                select.appendChild(option);
            });
        }
    } catch (error) {
        console.error('Ошибка загрузки самолётов:', error);
        showNotification('Ошибка загрузки списка самолётов', 'error');
    }
}

// ==================== ОБНОВЛЕННАЯ ИНИЦИАЛИЗАЦИЯ ====================

// Инициализация приложения
async function initApp() {
    // Загружаем все данные параллельно
    await Promise.all([
        loadFlights(), 
        loadAirports(), 
        loadAircraft()
    ]);
    
    // Обновляем UI
    loadFlightsTable();
    updateDashboard();
    loadCalendar();
    loadStatistics();
    
    // Настройки пользователя
    const role = checkAuthState();
    const username = localStorage.getItem("username");
    updateUserInterface(role, username);
    
    // Скрываем быстрые действия, если не staff
    const qa = document.getElementById("quick-actions-section");
    if (qa && role !== "staff") {
        qa.style.display = "none";
    }
    
    console.log('Приложение инициализировано');
}

// Функция открытия формы добавления рейса
window.openAddFlightForm = function() {
    showSection("add-flight");
    const now = new Date().toISOString().slice(0, 16);
    const departureInput = document.getElementById("departure-time");
    const arrivalInput = document.getElementById("arrival-time");
    if (departureInput) departureInput.value = now;
    if (arrivalInput) arrivalInput.value = now;
    
    const fields = ["flight-number", "departure-airport", "arrival-airport", "aircraft", "flight-terminal", "flight-gate"];
    fields.forEach(id => {
        const el = document.getElementById(id);
        if (el) el.value = "";
    });
    
    const statusSelect = document.getElementById("flight-status");
    if (statusSelect) statusSelect.value = "scheduled";
    const seatsInput = document.getElementById("seats");
    if (seatsInput) seatsInput.value = "150";
};

// Toggle export menu
window.toggleExportMenu = function() {
    const menu = document.getElementById('export-menu');
    if (menu) {
        menu.style.display = menu.style.display === 'none' ? 'block' : 'none';
    }
};

// Закрывать меню при клике вне
document.addEventListener('click', function(event) {
    const menu = document.getElementById('export-menu');
    const btn = event.target.closest('.dropdown button');
    if (menu && !btn && !menu.contains(event.target)) {
        menu.style.display = 'none';
    }
});

// Запуск приложения
document.addEventListener("DOMContentLoaded", async () => {
    await initApp();
    
    const editForm = document.getElementById("edit-flight-form");
    if (editForm) {
        editForm.addEventListener("submit", handleEditSubmit);
    }
    
    const addForm = document.getElementById("flight-form");
    if (addForm) {
        addForm.addEventListener("submit", addFlight);
    }
});

// Экспорт для использования в других модулях
export { initApp, loadAirports, loadAircraft };