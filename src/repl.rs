//! Sqlite - REPL
//! main loop

use std::{
    io::{self, Write},
    process::exit,
};

use crate::constants::*;
use crate::table::{Row, Table};

enum MetaState {
    SUCCESS,
    UNRECOGNIZED,
}

enum PrepareState {
    SUCCESS,
    UNRECOGNIZED,
    SyntaxError,
}

enum PrepareInsertResult {
    Success,
    SyntaxError,
    ParamsTooLong,
}

enum ExecuteResult {
    ExecuteSuccess,
    ExecuteTableFull,
}

#[derive(Debug)]
enum StatementType<'a> {
    INSERT(&'a str),
    SELECT(&'a str),
    UNKNOWN(&'a str),
    // Empty,
}

#[derive(Debug)]
struct Statement<'a> {
    stm_type: StatementType<'a>,
    row: Row,
}

impl<'a> Statement<'a> {
    fn new(stm_type: StatementType<'a>) -> Statement<'a> {
        Statement {
            // stm_type: StatementType::Empty,
            stm_type,
            row: Row::new(),
        }
    }
}

pub fn looper(filename: &str) -> io::Result<()> {
    // let mut table = Table::new();
    let mut table = Table::db_open(filename);
    let mut input_buf = String::new();
    loop {
        print!("sqlite > ");
        io::stdout().flush().unwrap();
        input_buf.clear();
        let _buf_len = io::stdin().read_line(&mut input_buf)?;
        input_buf.pop(); // pop "\n"
        if input_buf.starts_with(".") {
            match do_meta_cmd(&input_buf, &mut table) {
                MetaState::SUCCESS => {
                    continue;
                }
                MetaState::UNRECOGNIZED => {
                    println!("无法识别的命令: {}", &input_buf);
                    continue;
                }
            }
        }
        match check_stmt(&input_buf) {
            (PrepareState::SUCCESS, stm_type) => {
                let mut stmt = Statement::new(stm_type);
                execute_stmt(&mut stmt, &mut table);
                continue;
            }
            (PrepareState::SyntaxError, _) => {
                println!("语法错误: {}", &input_buf);
                continue;
            }
            (PrepareState::UNRECOGNIZED, _) => {
                println!("无法识别的语句: {}", &input_buf);
                continue;
            }
        }
    }
    // Ok(())
}

fn do_meta_cmd(cmd: &str, table: &mut Table) -> MetaState {
    if cmd == ".exit" || cmd == ".q" {
        table.db_close();
        exit(0)
    }
    if cmd == ".tables" {
        return MetaState::SUCCESS;
    }
    MetaState::UNRECOGNIZED
}

// fn prepare_stmt(stm_type: StatementType) -> Statement {
//     Statement::new(stm_type)
// }

fn check_stmt(buf: &str) -> (PrepareState, StatementType) {
    if buf.starts_with("insert") {
        let tokens = buf.split_whitespace().collect::<Vec<&str>>();
        if tokens.len() != 4 {
            return (PrepareState::SyntaxError, StatementType::INSERT(buf));
        }
        return (PrepareState::SUCCESS, StatementType::INSERT(buf));
    }
    if buf.starts_with("select") {
        return (PrepareState::SUCCESS, StatementType::SELECT(buf));
    }
    (PrepareState::UNRECOGNIZED, StatementType::UNKNOWN(buf))
}

fn execute_stmt(stmt: &mut Statement, table: &mut Table) {
    // println!("stmt: {:?}", stmt);

    match stmt.stm_type {
        StatementType::INSERT(s) => {
            // println!("insert stm: {}", s);
            if let PrepareInsertResult::Success = prepare_insert(&mut stmt.row, s) {
                execute_insert(&stmt.row, table);
                println!("Executed!");
            }
        }
        StatementType::SELECT(_) => {
            execute_select(table);
        }
        StatementType::UNKNOWN(_) => {}
    }
}

fn prepare_insert(row: &mut Row, input: &str) -> PrepareInsertResult {
    let tokens = input.split_whitespace().collect::<Vec<&str>>();
    if tokens.len() > 4 {
        println!("Expect 4 string, example: insert 1 xiaoming xiaoming@sina.com");
        return PrepareInsertResult::SyntaxError;
    }
    row.id = tokens[1].to_string().parse::<u32>().unwrap();

    let name_bytes = tokens[2].as_bytes();
    let namelen = name_bytes.len();
    if namelen > USERNAME_SIZE {
        println!("Username is too long!");
        return PrepareInsertResult::ParamsTooLong;
    }
    row.name[0..namelen].copy_from_slice(name_bytes);

    let email_bytes = tokens[3].as_bytes();
    let emaillen = email_bytes.len();
    if emaillen > EMAIL_SIZE {
        println!("Email is too long!");
        return PrepareInsertResult::ParamsTooLong;
    }
    row.email[0..emaillen].copy_from_slice(email_bytes);
    return PrepareInsertResult::Success;
}

fn execute_insert(row: &Row, table: &mut Table) -> ExecuteResult {
    if table.rows_count > TABLE_MAX_ROWS as u32 {
        return ExecuteResult::ExecuteTableFull;
    }
    
    // * 无光标
    let (page_num, byte_offsets) = table.row_slot(table.rows_count);
    table.serialize_row(&row, page_num, byte_offsets);

    // ! 双重可变引用错误
    // * 加入光标: 在表的末尾创建光标, 获取表中光标位置的偏移, 将记录数据序列化
    // let cursor = Cursor::table_end(table);
    // let (page_num, byte_offsets) = table.cursor_value(&cursor);
    // table.serialize_row(&row,page_num, byte_offsets);

    table.rows_count += 1;
    ExecuteResult::ExecuteSuccess
}

fn execute_select(table: &mut Table) -> ExecuteResult {
    for i in 0..table.rows_count {
        let (page_num, byte_offsets) = table.row_slot(i);
        &table.deserialize_row(page_num, byte_offsets).show();
    }
    ExecuteResult::ExecuteSuccess
}
