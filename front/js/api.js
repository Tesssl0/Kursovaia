// api.js

const API_BASE_URL = "http://localhost:8080";

// Обработка ошибок fetch
async function request(endpoint, options = {}) {
    const defaultHeaders = {
        "Content-Type": "application/json",
        "Role": localStorage.getItem("role") || ""
    };
    
    const config = {
        ...options,
        headers: { ...defaultHeaders, ...options.headers }
    };
    
    try {
        const response = await fetch(`${API_BASE_URL}${endpoint}`, config);
        
        if (!response.ok) {
            const error = await response.text();
            throw new Error(error || `HTTP ${response.status}`);
        }
        
        // Для DELETE и PUT без тела
        if (response.status === 204 || config.method === 'DELETE') {
            return { success: true };
        }
        
        return await response.json();
    } catch (error) {
        console.error('API Error:', error);
        throw error;
    }
}

// Рейсы
export const flightsAPI = {
    getAll: () => request('/flights'),
    
    create: (flightData) => request('/flights', {
        method: 'POST',
        body: JSON.stringify(flightData)
    }),
    
    update: (id, flightData) => request(`/flights/${id}`, {
        method: 'PUT',
        body: JSON.stringify(flightData)
    }),
    
    delete: (id) => request(`/flights/${id}`, {
        method: 'DELETE'
    }),
    
    updateStatus: (id, status) => request(`/flights/${id}/status`, {
        method: 'PUT',
        body: JSON.stringify({ status })
    }),
    
    import: (fileContent) => request('/flights/import', {
        method: 'POST',
        body: fileContent,
        headers: {
            "Content-Type": "text/plain",
            "Role": localStorage.getItem("role") || ""
        }
    })
};

// Аэропорты
export const airportsAPI = {
    getAll: () => request('/airports'),
    
    create: (data) => request('/airports', {
        method: 'POST',
        body: JSON.stringify(data)
    }),
    
    update: (id, data) => request(`/airports/${id}`, {
        method: 'PUT',
        body: JSON.stringify(data)
    }),
    
    delete: (id) => request(`/airports/${id}`, {
        method: 'DELETE'
    })
};

// Самолеты
export const aircraftAPI = {
    getAll: () => request('/aircraft'),
    
    create: (data) => request('/aircraft', {
        method: 'POST',
        body: JSON.stringify(data)
    }),
    
    update: (id, data) => request(`/aircraft/${id}`, {
        method: 'PUT',
        body: JSON.stringify(data)
    }),
    
    delete: (id) => request(`/aircraft/${id}`, {
        method: 'DELETE'
    })
};

// Статистика
export const statsAPI = {
    getFlightStatuses: () => request('/stats/flights'),
    getTerminals: () => request('/stats/terminals'),
    getUpcoming: () => request('/stats/upcoming')
};

// Авторизация
export const authAPI = {
    login: (username, password) => request('/login', {
        method: 'POST',
        body: JSON.stringify({ username, password })
    }),
    
    register: (username, password) => request('/register', {
        method: 'POST',
        body: JSON.stringify({ username, password })
    })
};