// ui.js

import { isStaff, logout } from './auth.js';
import { getAircraftName, getStatusText, formatDateTime } from './utils.js';

// Уведомления
export function showNotification(message, type = 'success') {
    const notification = document.getElementById('notification');
    const text = document.getElementById('notification-text');
    
    if (!notification) return;
    
    notification.className = 'notification';
    if (type === 'error') notification.classList.add('error');
    text.textContent = message;
    notification.style.display = 'flex';
    
    setTimeout(() => hideNotification(), 3000);
}

export function hideNotification() {
    const notification = document.getElementById('notification');
    if (notification) notification.style.display = 'none';
}

// Переключение секций
export function showSection(sectionId) {
    document.querySelectorAll('.content-section').forEach(s => s.classList.remove('active'));
    document.querySelectorAll('.nav-link').forEach(l => l.classList.remove('active'));
    
    const section = document.getElementById(sectionId);
    const navLink = document.querySelector(`[href="#${sectionId}"]`);
    
    if (section) section.classList.add('active');
    if (navLink) navLink.classList.add('active');
}

// Обновление интерфейса пользователя
export function updateUserInterface(role, username) {
    const userInfoDiv = document.querySelector(".user-info");
    if (!userInfoDiv) return;
    
    let roleText = "";
    
    switch(role) {
        case "staff":
            roleText = "Работник";
            document.querySelectorAll(".admin-only").forEach(el => el.style.display = "");
            break;
        case "user":
            roleText = "Пользователь";
            document.querySelectorAll(".admin-only").forEach(el => el.style.display = "none");
            break;
        default:
            roleText = "Гость";
            document.querySelectorAll(".admin-only").forEach(el => el.style.display = "none");
    }
    
    if (role !== "guest" && role) {
        userInfoDiv.innerHTML = `
            <i class="fas fa-user-circle"></i>
            <span>${username}</span>
            <span id="user-role-label">${roleText}</span>
            <button onclick="window.logoutHandler?.()" class="btn-small" style="margin-inline-start: 10px;">
                Выйти
            </button>
        `;
    } else {
        userInfoDiv.innerHTML = `
            <button onclick="window.openAuthModal?.()" class="btn-small">
                Вход / Регистрация
            </button>
        `;
    }
}

// Модальные окна
export function openModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) modal.style.display = "flex";
}

export function closeModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) modal.style.display = "none";
}

// Открытие модалки редактирования
export function openEditModal(flight) {
    document.getElementById("edit-id").value = flight.id;
    document.getElementById("edit-number").value = flight.number;
    document.getElementById("edit-origin").value = flight.origin;
    document.getElementById("edit-destination").value = flight.destination;
    document.getElementById("edit-departure").value = flight.departureTime.replace(" ", "T").slice(0, 16);
    document.getElementById("edit-arrival").value = flight.arrivalTime.replace(" ", "T").slice(0, 16);
    document.getElementById("edit-aircraft").value = flight.aircraft;
    document.getElementById("edit-seats").value = flight.seats;
    document.getElementById("edit-status").value = flight.status;
    document.getElementById("edit-terminal").value = flight.terminal || "";
    document.getElementById("edit-gate").value = flight.gate || "";
    
    openModal('edit-modal');
}

export function closeEditModal() {
    closeModal('edit-modal');
}

export function openAuthModal() {
    openModal('auth-modal');
}

export function closeAuthModal() {
    closeModal('auth-modal');
}

// Переключение табов в модалке
export function switchAuthTab(tab) {
    const loginBtn = document.getElementById("tab-login");
    const regBtn = document.getElementById("tab-register");
    const loginForm = document.getElementById("login-form");
    const regForm = document.getElementById("register-form");
    
    if (!loginBtn || !regBtn) return;
    
    if (tab === "login") {
        loginBtn.classList.add("active");
        regBtn.classList.remove("active");
        if (loginForm) loginForm.style.display = "flex";
        if (regForm) regForm.style.display = "none";
    } else {
        loginBtn.classList.remove("active");
        regBtn.classList.add("active");
        if (loginForm) loginForm.style.display = "none";
        if (regForm) regForm.style.display = "flex";
    }
}

// Обновление пагинации
export function updatePagination(currentPage, totalPages) {
    const pageInfo = document.getElementById('page-info');
    if (pageInfo) {
        pageInfo.textContent = `Страница ${currentPage} из ${totalPages}`;
    }
}