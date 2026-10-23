#ifndef ERRNO_H
#define ERRNO_H

/* Códigos de error estándar POSIX para Zenthra OS (Auditoría v15) */
#define EPERM            1      /* Operación no permitida */
#define ENOENT           2      /* No existe el archivo o directorio */
#define ESRCH            3      /* Proceso no encontrado */
#define EINTR            4      /* Llamada al sistema interrumpida */
#define EIO              5      /* Error de E/S física */
#define ENXIO            6      /* Dispositivo o dirección no configurada */
#define E2BIG            7      /* Lista de argumentos demasiado larga */
#define ENOEXEC          8      /* Formato ejecutable no válido */
#define EBADF            9      /* Descriptor de archivo no válido */
#define ECHILD          10      /* No existen procesos hijos */
#define EAGAIN          11      /* Recurso temporalmente no disponible / Sin datos en I/O no bloqueante */
#define EWOULDBLOCK     EAGAIN  /* Operación causaría bloqueo */
#define ENOMEM          12      /* Sin memoria suficiente disponible */
#define EACCES          13      /* Permiso denegado por seguridad ACL */
#define EFAULT          14      /* Dirección de memoria o puntero no válido */
#define EBUSY           16      /* Dispositivo o recurso ocupado */
#define EEXIST          17      /* Archivo o entidad ya existente */
#define EXDEV           18      /* Enlace entre dispositivos cruzados */
#define ENODEV          19      /* Dispositivo no soportado */
#define ENOTDIR         20      /* No es un directorio */
#define EISDIR          21      /* Es un directorio */
#define EINVAL          22      /* Argumento o parámetro no válido */
#define ENFILE          23      /* Límite de archivos del sistema alcanzado */
#define EMFILE          24      /* Demasiados archivos abiertos por el proceso */
#define ENOTTY          25      /* Operación de control I/O inapropiada */
#define EFBIG           27      /* Archivo demasiado grande */
#define ENOSPC          28      /* Espacio de almacenamiento agotado */
#define ESPIPE          29      /* Posicionamiento ilegal en tubería/stream */
#define EROFS           30      /* Sistema de archivos de solo lectura */
#define EMLINK          31      /* Demasiados enlaces */
#define EPIPE           32      /* Tubería rota */
#define EDOM            33      /* Argumento numérico fuera de dominio */
#define ERANGE          34      /* Resultado numérico fuera de rango */
#define ENAMETOOLONG    36      /* Nombre de archivo demasiado largo */
#define ENOSYS          38      /* Llamada al sistema o función no implementada */
#define ENOTEMPTY       39      /* Directorio no vacío */
#define EOVERFLOW       75      /* Desbordamiento aritmético de valor */

#endif
