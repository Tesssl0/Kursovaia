// utils.js

// Преобразует значение из datetime-local в объект Date
export function parseLocalDateTime(value) {
    if (!value) return null;
    return new Date(value.replace(" ", "T"));
}

// Нормализация datetime-local для разных браузеров
export function normalizeDateTime(dateTimeStr) {
    return dateTimeStr.replace("T", " ").slice(0, 16);
}

// Получить понедельник недели
export function getMonday(date) {
    const d = new Date(date);
    const day = d.getDay();
    const diff = d.getDate() - (day === 0 ? 6 : day - 1);
    return new Date(d.setDate(diff));
}

// Форматирование даты
export function formatDateTime(date, format = 'datetime') {
    const d = new Date(date);
    if (format === 'time') {
        return d.toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
    }
    if (format === 'date') {
        return d.toLocaleDateString('ru-RU', { day: 'numeric', month: 'long' });
    }
    return d.toLocaleString('ru-RU');
}

// Получить название самолета по ID
export function getAircraftName(id) {
    const aircraft = {
        1: "Airbus A320",
        2: "Boeing 737-800",
        3: "Sukhoi Superjet 100"
    };
    return aircraft[id] || "Неизвестно";
}

// Получить текст статуса
export function getStatusText(status) {
    const statuses = {
        'scheduled': 'По расписанию',
        'delayed': 'Задержан',
        'departed': 'Вылетел',
        'arrived': 'Прибыл',
        'cancelled': 'Отменен'
    };
    return statuses[status] || status;
}

// Проверка пересечения интервалов
export function intervalsOverlap(aStart, aEnd, bStart, bEnd) {
    return aStart < bEnd && aEnd > bStart;
}

// Форматирование даты для SQL
export function formatForSQL(dateTimeStr) {
    return dateTimeStr.replace("T", " ") + ":00";
}

// Задержка для debounce
export function debounce(func, delay) {
    let timeout;
    return function (...args) {
        clearTimeout(timeout);
        timeout = setTimeout(() => func.apply(this, args), delay);
    };
}