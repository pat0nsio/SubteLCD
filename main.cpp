#include <iostream>
#include <string>
#include <cstdlib> // Para std::getenv

int main() {
    // 1. Leer las variables del entorno del OS
    // std::getenv devuelve un puntero C (char*), o NULL si no existe.
    const char* id_ptr = std::getenv("CLIENT_ID");
    const char* secret_ptr = std::getenv("CLIENT_SECRET");

    // 2. Validar que las variables existan
    if (id_ptr == nullptr || secret_ptr == nullptr) {
        std::cerr << "Error: Las variables CLIENT_ID o CLIENT_SECRET no están definidas." << std::endl;
        std::cerr << "Ejecuta: source .env && ./tu_programa" << std::endl;
        return 1; // Termina con error
    }

    // 3. Convertir a std::string de C++
    std::string client_id = id_ptr;
    std::string client_secret = secret_ptr;

    // 4. Usarlas en tu lógica
    std::string url = "https://apitransporte.buenosaires.gob.ar/subtes/feed-gtfs?client_id="
                      + client_id + "&client_secret=" + client_secret;

    std::cout << "Consultando API con ID: " << client_id.substr(0, 4) << "..." << std::endl;
    std::cout << "URL: " << url << std::endl;

    // (Aquí iría tu código de red, ej: cURLpp o Boost.Beast)

    return 0;
}