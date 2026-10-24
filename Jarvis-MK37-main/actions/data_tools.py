"""
Data Tools - Herramientas de análisis factual y descriptivo para el Agente de Datos
Regla estricta: No inventa ni alucina datos; muestra supuestos y límites de la muestra.
"""

import csv
import io
import math
from typing import Dict, List, Any


def analyze_csv_structure(csv_text: str) -> Dict[str, Any]:
    """Parsea un texto CSV y extrae metadata de columnas, tipos y filas."""
    f = io.StringIO(csv_text.strip())
    reader = csv.reader(f)
    try:
        headers = next(reader)
    except StopIteration:
        return {"error": "CSV vacío o sin encabezados", "row_count": 0}

    rows = list(reader)
    col_counts = len(headers)
    null_counts = {h: 0 for h in headers}

    for row in rows:
        for idx, val in enumerate(row):
            if idx < col_counts:
                if not val.strip():
                    null_counts[headers[idx]] += 1

    return {
        "headers": headers,
        "total_columns": col_counts,
        "total_rows": len(rows),
        "null_counts": null_counts,
        "sample_preview": rows[:3] if rows else []
    }


def compute_descriptive_statistics(numbers: List[float]) -> Dict[str, Any]:
    """Calcula estadísticas descriptivas exactas de una muestra numérica."""
    if not numbers:
        return {"error": "Conjunto de datos vacío"}

    n = len(numbers)
    sorted_nums = sorted(numbers)
    total_sum = sum(sorted_nums)
    mean = total_sum / n

    # Mediana
    if n % 2 == 1:
        median = sorted_nums[n // 2]
    else:
        median = (sorted_nums[n // 2 - 1] + sorted_nums[n // 2]) / 2.0

    # Varianza y desviación estándar
    variance = sum((x - mean) ** 2 for x in sorted_nums) / n
    std_dev = math.sqrt(variance)

    # Cuartiles
    q1 = sorted_nums[int(n * 0.25)]
    q3 = sorted_nums[int(n * 0.75)]
    iqr = q3 - q1

    return {
        "count": n,
        "sum": round(total_sum, 4),
        "mean": round(mean, 4),
        "median": round(median, 4),
        "variance": round(variance, 4),
        "std_dev": round(std_dev, 4),
        "min": sorted_nums[0],
        "max": sorted_nums[-1],
        "q1": q1,
        "q3": q3,
        "iqr": round(iqr, 4)
    }


def validate_sql_syntax(sql_text: str) -> Dict[str, Any]:
    """Valida la sintaxis básica y mejores prácticas de consultas SQL."""
    sql_clean = sql_text.strip()
    sql_upper = sql_clean.upper()

    has_select = "SELECT" in sql_upper
    has_from = "FROM" in sql_upper
    has_where = "WHERE" in sql_upper
    is_delete_without_where = "DELETE" in sql_upper and not has_where
    is_update_without_where = "UPDATE" in sql_upper and not has_where

    risks = []
    if is_delete_without_where:
        risks.append("DELETE sin cláusula WHERE (eliminaría todos los registros de la tabla).")
    if is_update_without_where:
        risks.append("UPDATE sin cláusula WHERE (modificaría todos los registros de la tabla).")
    if "SELECT *" in sql_upper:
        risks.append("Uso de 'SELECT *': se recomienda especificar explícitamente las columnas necesarias.")

    return {
        "is_valid_query": (has_select and has_from) or "INSERT" in sql_upper or "UPDATE" in sql_upper or "DELETE" in sql_upper,
        "has_filter": has_where,
        "risks": risks,
        "is_safe": len(risks) == 0 or (len(risks) == 1 and "SELECT *" in sql_upper)
    }
