use std::io::{self, Write};

fn read_line(prompt: &str) -> Result<String, String> {
    print!("{prompt}");
    io::stdout().flush().map_err(|e| e.to_string())?;

    let mut line = String::new();
    if io::stdin().read_line(&mut line).map_err(|e| e.to_string())? == 0 {
        return Err("입력이 종료되었습니다.".into());
    }
    Ok(line)
}

fn read_size(name: &str) -> Result<(usize, usize), String> {
    let line = read_line(&format!("{name}의 행과 열 입력: "))?;
    let size: Vec<usize> = line
        .split_whitespace()
        .map(|s| s.parse::<usize>())
        .collect::<Result<_, _>>()
        .map_err(|_| "행과 열은 양의 정수로 입력해야 합니다.".to_string())?;

    if size.len() != 2 || size[0] == 0 || size[1] == 0 {
        return Err("행과 열을 0보다 큰 정수 두 개로 입력하세요.".into());
    }

    let count = size[0]
        .checked_mul(size[1])
        .ok_or("행렬 크기가 너무 큽니다.")?;

    if count > 1_000_000 {
        return Err("행렬의 원소 수는 1,000,000개 이하여야 합니다.".into());
    }

    Ok((size[0], size[1]))
}

fn read_matrix(name: &str, rows: usize, cols: usize) -> Result<Vec<i64>, String> {
    let mut matrix = Vec::with_capacity(rows * cols);
    println!("{name}의 원소를 한 행씩 입력하세요.");

    for row in 0..rows {
        let line = read_line(&format!("{}행: ", row + 1))?;
        let values: Vec<i64> = line
            .split_whitespace()
            .map(|s| s.parse::<i64>())
            .collect::<Result<_, _>>()
            .map_err(|_| "원소는 i64 범위의 정수로 입력해야 합니다.".to_string())?;

        if values.len() != cols {
            return Err(format!("각 행에는 원소를 {cols}개 입력해야 합니다."));
        }
        matrix.extend(values);
    }

    Ok(matrix)
}

fn run() -> Result<(), String> {
    println!("실습 2: Rust 행렬 덧셈");

    let (rows_a, cols_a) = read_size("행렬 A")?;
    let (rows_b, cols_b) = read_size("행렬 B")?;

    if rows_a != rows_b || cols_a != cols_b {
        return Err("두 행렬의 크기가 달라 덧셈할 수 없습니다.".into());
    }

    let a = read_matrix("행렬 A", rows_a, cols_a)?;
    let b = read_matrix("행렬 B", rows_b, cols_b)?;
    let mut result = Vec::with_capacity(a.len());

    for (x, y) in a.iter().zip(b.iter()) {
        result.push(x.checked_add(*y).ok_or("덧셈 결과가 i64 범위를 벗어납니다.")?);
    }

    println!("덧셈 결과 (A + B):");
    for row in result.chunks(cols_a) {
        for value in row {
            print!("{value}\t");
        }
        println!();
    }

    Ok(())
}

fn main() {
    if let Err(message) = run() {
        eprintln!("오류: {message}");
        std::process::exit(1);
    }
}
