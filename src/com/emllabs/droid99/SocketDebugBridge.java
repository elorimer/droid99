package com.emllabs.droid99;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.io.FileOutputStream;
import android.content.Context;

import android.util.Log;

public class SocketDebugBridge implements Runnable {
	protected ServerSocket serverSocket;
	protected TI99Simulator simulator;
	protected OutputStreamWriter out;
	protected boolean inLuaCode;
	protected String luaCode;
	protected Context ctx;
	protected int contLoop;
	protected String[] GPLOpcodes = { "RTN", "RTNC", "RAND", "SCAN", "BACK",
			"B", "CALL", "ALL", "FMT", "H", "GT", "EXIT", "CARRY", "OVF",
			"PARSE", "XML", "CONT", "EXEC", "RTNB", "XGPL", "XGPL", "XGPL",
			"XGPL", "XGPL", "XGPL", "XGPL", "XGPL", "XGPL", "XGPL", "XGPL",
			"XGPL", };
	protected String[] GPLOpcodesExt = { "ABS", "DABS", "NEG", "DNEG", "INV",
			"DINV", "CLR", "DCLR", "FETCH", "FETCH", "CASE", "DCASE", "PUSH",
			"PUSH", "CZ", "DCZ", "INC", "DINC", "DEC", "DDEC", "INCT", "DINCT",
			"DECT", "DDECT", };

	protected String[] mnemonics = new String[] { "A   ", "AB  ", "C   ",
			"CB  ", "S   ", "SB  ", "SOC ", "SOCB", "SZC ", "SZCB", "MOV ",
			"MOVB", "COC ", "CZC ", "XOR ", "MPY ", "DIV ", "XOP ", "B   ",
			"BL  ", "BLWP", "CLR ", "SETO", "INV ", "NEG ", "ABS ", "SWPB",
			"INC ", "INCT", "DEC ", "DECT", "X   ", "LDCR", "STCR", "SBO ",
			"SBZ ", "TB  ", "JEQ ", "JGT ", "JH  ", "JHE ", "JL  ", "JLE ",
			"JLT ", "JMP ", "JNC ", "JNE ", "JNO ", "JOC ", "JOP ", "SLA ",
			"SRA ", "SRC ", "SRL ", "AI  ", "ANDI", "CI  ", "LI  ", "ORI ",
			"LWPI", "LIMI", "STST", "STWP", "RTWP", "IDLE", "RSET", "CKOF",
			"CKON", "LREX" };

	public SocketDebugBridge(int port, TI99Simulator sim, Context context) {
		ctx = context;
		simulator = sim;
		contLoop = 0;
		try {
			serverSocket = new ServerSocket(port);
		} catch (IOException e) {
			Log.e("And99", "FAILED to create server socket");
			// do something
		}
	}

	public void trap() {
		String regs = simulator.DumpRegisters();
		try {
			out.write("\nException:\n" + regs);
			out.write("GROM: "
					+ Integer.toHexString(simulator.GetGROMAddress()) + " = "
					+ Integer.toHexString(simulator.GetGROMByte()) + "\n");
			out.write("#> ");
			out.flush();
		} catch (IOException e) {
			Log.i("And99", "ouput exception");
		}
		// clear the exception so we can single step again
		simulator.ClearException();
		if (contLoop > 0) {
			contLoop--;
			simulator.continueCPU();
		}
	}

	public static char[] byteToString(byte val) {
		char[] out = new char[2];
		char[] hexDigits = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
				'A', 'B', 'C', 'D', 'E', 'F' };
		int b = val & 0xFF;
		out[0] = hexDigits[b >> 4];
		out[1] = hexDigits[b & 0xF];

		return out;
	}

	protected String disassembleGPL(byte opcode) {
		int p = opcode & 0xff;
		if (p < GPLOpcodes.length)
			return GPLOpcodes[p];
		if (p <= 0x3F)
			return "MOVE";
		if (p <= 0x5f)
			return "BR";
		if (p <= 0x7f)
			return "BS";

		if (p <= 0x97)
			return GPLOpcodesExt[p - 0x80];
		return "XXX";
	}
	

	protected boolean handleLine(String line) throws IOException
	{
		if (inLuaCode) {
			if (line.length() > 0 && line.charAt(line.length() - 1) == ';') {
				luaCode += line.substring(0, line.length() - 1);
				ExecuteLua(luaCode);
				luaCode = "";
				out.write("\n");
				return false;
			}
			if (line.equals(".")) {
				inLuaCode = false;
				ExecuteLua(luaCode);
				luaCode = "";
				return false;
			}
			luaCode += line + "\n";
			return false;
		}

		String[] tokens = line.split("[ ]+");

		if (tokens[0].startsWith("cont")) {
			if (tokens.length > 1) {
				contLoop = Integer.parseInt(tokens[1], 10);
			}
			simulator.continueCPU();
			return false;
		}
		if (tokens[0].startsWith("break")) {
			simulator.breakCPU();
			out.write(simulator.DumpRegisters());
			return false;
		}
		if (tokens[0].startsWith("step")) {
			simulator.StepCPU();
			out.write(simulator.DumpRegisters());
			out.write("GROM: "
					+ Integer.toHexString(simulator.GetGROMAddress()) + " = "
					+ Integer.toHexString(simulator.GetGROMByte()) + "\n");
			return false;
		}
		if (tokens[0].startsWith("mem")) {
			byte[] ram = simulator.ReadCPUMemory();
			for (int i = 0; i < 256; i++) {
				out.write(byteToString(ram[i]));
				out.write(' ');
			}
			out.write("\n");
			return false;
		}
		if (tokens[0].startsWith("save")) {
			// XXX: Make sure simulator is stopped!!
			simulator.SaveImage();
			return false;
		}
		if (tokens[0].startsWith("load")) {
			// XXX: Make sure simulator is stopped!!
			simulator.LoadImage();
			return false;
		}
		if (tokens[0].startsWith("gpl")) {
			if (tokens.length > 1) {
				int address = Integer.parseInt(tokens[1], 16);
				simulator.debugGROM = address;
			} else {
				// Single step GPL
				simulator.setBreakpoint(0x70);
				simulator.singleStepGPL = true;
			}
			return false;
		}
		if (tokens[0].startsWith("pc")) {
			// Check usage
			int address = Integer.parseInt(tokens[1], 16);
			simulator.setBreakpoint(address);
			return false;
		}
		if (tokens[0].startsWith("nogpl")) {
			simulator.debugGROM = 0;
			simulator.singleStepGPL = false;
			return false;
		}
		if (tokens[0].startsWith("rdw")) { // read word
			if (tokens.length < 2) {
				out.write("Usage: rdw <address>\n");
				return false;
			}
			byte[] memory = simulator.ReadCPUMemory();
			int address = Integer.parseInt(tokens[1], 16);
			// yeah don't pass something out of range (00-FF). This is just CPU
			// memory.
			// make another function to read VDP and I/O memory ...
			char[] b1 = byteToString(memory[address]);
			char[] b2 = byteToString(memory[address + 1]);
			out.write("0x");
			out.write(b1);
			out.write(b2);
			out.write("\n");
			return false;
		}
		if (tokens[0].startsWith("rdb")) { // read byte
			if (tokens.length < 2) {
				out.write("Usage: rdb <address>\n");
				return false;
			}
			byte[] memory = simulator.ReadCPUMemory();
			int address = Integer.parseInt(tokens[1], 16);
			// yeah don't pass something out of range (00-FF). This is just CPU
			// memory.
			// make another function to read VDP and I/O memory ...
			char[] b1 = byteToString(memory[address]);
			out.write("0x");
			out.write(b1);
			out.write("\n");
			return false;
		}
		
		if (tokens[0].startsWith("vdp")) {
			if (tokens.length < 2) {
				out.write("Usage: vdp <regs|image|counts|clear>\n");
				return false;
			}
			if (tokens[1].startsWith("reg")) {
				byte[] registers = simulator.GetVDPRegisters();
				for (int i = 0; i < 8; i++) {
					out.write("0x");
					out.write(byteToString(registers[i]));
					out.write(" ");
				}
				out.write("\n");
			}
			if (tokens[1].startsWith("image")) {
				byte[] registers = simulator.GetVDPRegisters();
				ByteBuffer vdpMemory = simulator.vdpMemory;
				byte[] vdpBytes = vdpMemory.array();
				try {
					FileOutputStream os = ctx.openFileOutput("vdp.img",
							Context.MODE_PRIVATE);
					os.write(vdpBytes);
					os.write(registers);
					os.close();
					out.write("Wrote VDP image file\n");
				} catch (Exception e) {
					out.write("Failed to write image to file\n");
				}
			}
			if (tokens[1].startsWith("clear")) {
				simulator.ClearVDPCounts();
			}
			if (tokens[1].startsWith("counts")) {
				int[] counts = simulator.GetVDPCounts();
				for (int i = 0; i < 16384; i++) {
					if (counts[i] > 0)
						out.write(i + ": " + counts[i] + "\n");
				}
				out.write("Trace PCs:\n");
				for (int i = 16384; i < 16384 + 16; i++)
					out.write(counts[i] + " ");
				out.write("\n");
			}
			return false;
		}
		if (tokens[0].startsWith("icounts")) {
			if (tokens.length < 2) {
				out.write("Usage: icounts <view|reset>\n");
				return false;
			}
			if (tokens[1].startsWith("view")) {
				int[] counts = simulator.GetICounts();
				for (int i = 0; i < counts.length; i++)
					if (counts[i] > 0)
						out.write(mnemonics[i] + ": " + counts[i] + "\n");
				int start = Integer.valueOf(tokens[2]).intValue();
				int stop = Integer.valueOf(tokens[3]).intValue();
				for (int i = start; i < stop; i++)
					if (simulator.GPLCounts[i] > 0) {
						out.write(i + ": " + simulator.GPLCounts[i] + ": ");
						if (i < 0x6000)
							out.write(disassembleGPL(simulator.consoleGROM[i]));
						else
							out.write(disassembleGPL(simulator.cartGROM[i - 0x6000]));
						out.write("\n");
					}
			} else {
				for (int i = 0; i < 0x6000 + 32768; i++)
					simulator.GPLCounts[i] = 0;
				simulator.ResetICounts();
			}
			return false;
		}
		if (tokens[0].startsWith("lua")) {
			inLuaCode = true;
			luaCode = "";
			return false;
		}
		return true;
	}

	public void run() {
		while (true) {
			boolean finished = false;
			inLuaCode = false;
			try {
				Socket client = serverSocket.accept();
				simulator.attachDebugger(this);
				BufferedReader in = new BufferedReader(new InputStreamReader(
						client.getInputStream()));
				out = new OutputStreamWriter(client.getOutputStream());
				simulator.breakCPU();
				StartupLua(out);
				out.write("Android TI-99 Debug Bridge v.1\n");
				out.write(simulator.DumpRegisters());
				while (!finished) {
					if (!inLuaCode)
						out.write("#> ");
					else
						out.write("... ");
					out.flush();
					String str = in.readLine();
					if (str == null)
						break;
					finished = handleLine(str);
				}
				simulator.detachDebugger();
				client.close();
			} catch (IOException e) {
				Log.e("And99", "IOException communicating with client");
			}
		}
	}

	protected native void StartupLua(OutputStreamWriter w);
	protected native void ExecuteLua(String s);
}
