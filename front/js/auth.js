// auth.js

import { authAPI } from './api.js';
import { showNotification, updateUserInterface, showSection } from './ui.js';  // Добавлен showSection

// Проверка состояния авторизации
export function checkAuthState() {
    const role = localStorage.getItem("role");
    const username = localStorage.getItem("username");
    
    if (!role && !username) {
        document.querySelectorAll(".admin-only").forEach(el => el.style.display = "none");
        return "guest";
    }
    
    return role;
}

// Проверка прав сотрудника
export function isStaff() {
    return localStorage.getItem("role") === "staff";
}

// Проверка авторизации
export function isAuthenticated() {
    return !!localStorage.getItem("role");
}

// Вход в систему
export async function login(username, password) {
    try {
        const data = await authAPI.login(username, password);
        
        localStorage.setItem("username", data.username);
        localStorage.setItem("role", data.role);
        
        return { success: true, data };
    } catch (error) {
        return { success: false, error: error.message };
    }
}

// Регистрация
export async function register(username, password, password2) {
    // Валидация
    if (username.length < 4) {
        return { success: false, error: "Логин должен быть не короче 4 символов" };
    }
    
    if (password.length < 6) {
        return { success: false, error: "Пароль должен быть не короче 6 символов" };
    }
    
    if (password !== password2) {
        return { success: false, error: "Пароли должны совпадать" };
    }
    
    try {
        const data = await authAPI.register(username, password);
        return { success: true, data };
    } catch (error) {
        return { success: false, error: error.message };
    }
}

// Выход из системы
export function logout() {
    localStorage.removeItem("role");
    localStorage.removeItem("username");
    showNotification("Вы вышли из системы");
    
    // Обновляем интерфейс
    updateUserInterface("guest", "");
    
    // Возвращаем на главную
    showSection('dashboard');  
}

// Получить текущего пользователя
export function getCurrentUser() {
    return {
        username: localStorage.getItem("username"),
        role: localStorage.getItem("role")
    };
}