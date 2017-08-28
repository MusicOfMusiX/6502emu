#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
 
//We are not using instruction functions
//Instead, the execute function will look up these marcos with a gigantic switch

#define ADC 0
#define AND 1
#define ASL 2
#define BCC 3
#define BCS 4
#define BEQ 5
#define BIT 6
#define BMI 7
#define BNE 8
#define BPL 9
#define BRK 10
#define BVC 11
#define BVS 12
#define CLC 13
#define CLD 14
#define CLI 15
#define CLV 16
#define CMP 17
#define CPX 18
#define CPY 19
#define DEC 20
#define DEX 21
#define DEY 22
#define EOR 23
#define INC 24
#define INX 25
#define INY 26
#define JMP 27
#define JSR 28
#define LDA 29
#define LDX 30
#define LDY 31
#define LSR 32
#define NOP 33
#define ORA 34
#define PHA 35
#define PHP 36
#define PLA 37
#define PLP 38
#define ROL 39
#define	ROR 40
#define RTI 41
#define RTS 42
#define SBC 43
#define SEC 44
#define SED 45
#define SEI 46
#define STA 47
#define STX 48
#define STY 49
#define TAX 50
#define TAY 51
#define TSX 52
#define TXA 53
#define TXS 54
#define	TYA 55

//Define addressing mode marcos
#define IMM 100
#define IMP 101
#define ACC 102
#define ZERO 103
#define ZEROX 104
#define ZEROY 105
#define REL 106
#define ABS 107
#define ABSX 108
#define ABSY 109
#define INDIR 110
#define INDIRX 111 //Indexed indirect
#define INDIRY 112 //Indirect indexed
#define NOADDR 113 //The NOP equivalent for addressing modes

char * instructionnames[56] = {"ADC","AND","ASL","BCC","BCS","BEQ","BIT","BMI","BNE","BPL","BRK","BVC","BVS","CLC","CLD","CLI","CLV","CMP","CPX","CPY","DEC","DEX","DEY","EOR","INC","INX","INY","JMP","JSR","LDA","LDX","LDY","LSR","NOP","ORA","PHA","PHP","PLA","PLP","ROL","ROR","RTI","RTS","SBC","SEC","SED","SEI","STA","STX","STY","TAX","TAY","TSX","TXA","TXS","TYA"};
char * addrmodenames[14] =  {"IMM","IMP","ACC","ZERO","ZEROX","ZEROY","REL","ABS","ABSX","ABSY","INDIR","INDIRX","INDIRY","NOADDR"};

uint16_t PC; //Program counter
uint8_t CPUMEM[0x10000]; //Even though negative numbers exist, we can use unsigned numbers! ('Cause we're gonna use two's complement!)

uint8_t SP = 0xFF; //Stack pointer. The stack is located @ $0100-$01FF should access topmost data as CPUMEM[0x100 + SP]

uint8_t A; //Accumulator
uint8_t X; //X register
uint8_t Y; //Y register
uint8_t P; //Processor status

int group0opcode[0x8] = {NOP,BIT,JMP,JMP,STY,LDY,CPY,CPX};
int group0addrmode[0x8] = {IMM,ZERO,NOADDR,ABS,NOADDR,ZEROX,NOADDR,ABSX}; //BOTH JMPS HAVE ABS, BUT ONE IS INDIR.
int group0cyclecount[0x8] =  {2,3,0,4,0,4,0,4};

int group1opcode[0x8] = {ORA,AND,EOR,ADC,STA,LDA,CMP,SBC};
int group1addrmode[0x8] = {INDIRX,ZERO,IMM,ABS,INDIRY,ZEROX,ABSY,ABSX};
int group1cyclecount[0x8] =  {6,3,2,4,5,4,4,4};

int group2opcode[0x8] = {ASL,ROL,LSR,ROR,STX,LDX,DEC,INC};
int group2addrmode[0x8] = {IMM,ZERO,ACC,ABS,NOADDR,ZEROX,NOADDR,ABSX}; //STX HAS ZEROY INSTEAD OF ZEROX. LDA HAS ABSY INSTEAD OF ABSX.
int group2cyclecount[0x8] =  {0,5,2,6,0,6,0,7}; //STX AND LDX FOLLOW GROUP0 RULES 

//Branch instructions - does not follow aaab bbcc rule - xxy1 0000 rule
int group3opcode[0x8] = {BPL,BMI,BVC,BVS,BCC,BCS,BNE,BEQ};
//Branch instructions all use REL mode
//Branch instructions take 2 cycles when no branch was taken, 3 when taken, and has an additional cycle when page crossed

//Other single-byte instructions - **** 1000 scheme
int group4opcode[0x10] = {PHP,CLC,PLP,SEC,PHA,CLI,PLA,SEI,DEY,TYA,TAY,CLV,INY,CLD,INX,SED};
//Group4 all use IMP mode
int group4cyclecount[0x10] = {3,2,4,2,3,2,4,2,2,2,2,2,2,2,2,2};

//Other single-byte instructions - 1*** 1010 scheme
int group5opcode[0x7] = {TXA,TXS,TAX,TSX,DEX,NOP,NOP}; //$DA is missing, just replacing w/ NOP
//Group5 all use IMP mode
//Group5 all take 2 cycles to execute

//Other single-byte instructions: BRK,RTI,RTS, and JSR will be handled separately

//Function init here
int decodeandexecute();
struct operandstruct getoperand(int addrmode, int instruction, int opcode);
int exec(int instruction, int operand, int addrmode, int opcode);
int getcyclecount(int cycle, int pagecross, int branch, int opcode);

//Processor status functions
void setznflag(uint8_t testregister);

void setflag(uint8_t flagno,int value);
int getflag(uint8_t flagno);

//Stack functions
void stackpush(uint8_t value);
uint8_t stackpull();


struct operandstruct
	{
	int operand;
	int pagecross;
	};
	

//Function bodies here

//##############################################MAIN####################################################
int main(int argc, char *argv[])
	{
	setflag(5,1);
	//Do all initialisation here
	memset(CPUMEM,0,sizeof(CPUMEM));	
	int remainingcycles = 1;
	PC = 0x4000;
	/*
	CPUMEM[0x4000] = 0x38;
	CPUMEM[0x4001] = 0xA9;
	CPUMEM[0x4002] = 0x33;
	CPUMEM[0x4003] = 0x2A;
	
	
	CPUMEM[0] = 0xA9; //LDA IMM
	CPUMEM[1] = 0xE2;
	CPUMEM[2] = 0x6D; //ADC ABS
	CPUMEM[3] = 0x78; 
	CPUMEM[4] = 0x2A;
	CPUMEM[0x2A78] = 0x64;
	*/
	/*
	CPUMEM[0] = 0xA9; //LDA IMM
	CPUMEM[1] = 0xE2;
	CPUMEM[2] = 0x69; //ADC IMM
	CPUMEM[3] = 0x78; 
	*/
	
	FILE * ROM = fopen("AllSuiteA.bin", "rb");
	if(!ROM)
		{
        printf("ERROR WHILE OPENING ROM FILE\n\n");
        fflush(stdin); 
        getchar();
        exit(1);
    	}
    	
    fread(CPUMEM+0x4000,1,0x10000-0x4000,ROM);	
    
	while(1)
		{
		if(remainingcycles == 1) //Don't set to zero because one cycle is already used(?)
			{
			//getchar();
			printf("\n\n%04X ",PC);
			remainingcycles = decodeandexecute();
			}
		if (remainingcycles > 1)
			{
			remainingcycles -= 1;
			}
		if(PC == 0x45C0)
			{
			printf("\n\nPC reached 0x45C0. Test program end.");
			printf("\nMemory location 0x0210: %02X",CPUMEM[0x0210]);
			break;	
			}
		}
	return 0;
	}

//######################################################################################################

void setznflag(uint8_t testregister)
	{
	setflag(1,0);
	setflag(7,0);
	if(testregister == 0) {setflag(1,1);}
	if((testregister & 0x80) == 0x80) {setflag(7,1);}
	}

int getflag(uint8_t flagno)
	{
	return ((P >> flagno) & 0x1);
	}
	
void setflag(uint8_t flagno, int value)
	{
	if(value == 1)
		{
		P |= (1 << flagno);
		}
	if(value == 0)
		{
		P &= ~(1 << flagno);
		}
	}

void stackpush(uint8_t value) //The stack pointer points to the address where a new push should be stored, therefore CPUMEM[0x0100+SP] should not contaian data. (This seems to be a bit different to the C8 stack, but this method makes more sense.)
	{
	CPUMEM[0x0100+(SP--)] = value; //Wraparound is automatically done by SP
	}
	
uint8_t stackpull()
	{
	return CPUMEM[0x0100+(++SP)];
	}

int decodeandexecute()
	{
	//printf("\n--------------------------------------------------------------------------\n");
	//printf("\n#####Decode start#####\n");
	//printf("\n%04X ",PC);
	
	int group;
	int opcode = CPUMEM[PC];
	
	printf("\nopcode: %02X, %02X, %02X\n",opcode,CPUMEM[PC+1],CPUMEM[PC+2]);
	
	//Find group
	if((opcode & 0x1F) == 0x10) {group = 3;}
	if((opcode & 0xF) == 0x8) {group = 4;}
	if((opcode & 0x8F) == 0x8A) {group = 5;}
	if((group  != 3) && (group  != 4) && (group  != 5))
		{
		group = (opcode & 0x3);	
		}
	if((opcode == 0x00) || (opcode == 0x40) || (opcode == 0x60) || (opcode == 0x20)) {group = 6;} //BRK,RTI,RTS, and JSR
	
	//printf("\ngroup: %d", group);
	
	uint8_t aaa,bbb;
	int instruction, addrmode, remainingcycles, branch;
	
	struct operandstruct operandinfo;
	
	switch(group)
		{
		case 0:
			aaa = (opcode & 0xE0) >> 5;
			bbb = (opcode & 0x1C) >> 2;
			
			instruction = group0opcode[aaa];
			
			addrmode = group0addrmode[bbb];

			operandinfo = getoperand(addrmode,instruction,opcode); //Produces operandinfo->operand and operandinfo->pagecross
			
			////printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
			printf("%s %04X (%s)",instructionnames[instruction],operandinfo.operand,addrmodenames[addrmode-100]);
			branch = exec(instruction,operandinfo.operand,addrmode,opcode);
			remainingcycles = getcyclecount(group0cyclecount[bbb], operandinfo.pagecross, branch, opcode);
		break;
		case 1:
			aaa = (opcode & 0xE0) >> 5;
			bbb = (opcode & 0x1C) >> 2;
			
			instruction = group1opcode[aaa];
			addrmode = group1addrmode[bbb];
			
		    operandinfo = getoperand(addrmode,instruction,opcode); //Produces operandinfo->operand and operandinfo->pagecross
		    
		    //printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
		    printf("%s %04X (%s)",instructionnames[instruction],operandinfo.operand,addrmodenames[addrmode-100]);
			branch = exec(instruction,operandinfo.operand,addrmode,opcode);
			remainingcycles = getcyclecount(group1cyclecount[bbb], operandinfo.pagecross, branch, opcode);
		break;
		case 2:
			aaa = (opcode & 0xE0) >> 5;
			bbb = (opcode & 0x1C) >> 2;
			
			instruction = group2opcode[aaa];
			addrmode = group2addrmode[bbb];
			
			operandinfo = getoperand(addrmode,instruction,opcode); //Produces operandinfo->operand and operandinfo->pagecross
			
			//printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
			printf("%s %04X (%s)",instructionnames[instruction],operandinfo.operand,addrmodenames[addrmode-100]);
			
			branch = exec(instruction,operandinfo.operand,addrmode,opcode);
			remainingcycles = getcyclecount(group2cyclecount[bbb], operandinfo.pagecross, branch, opcode);
		break;
		case 3:
			aaa = (opcode & 0xE0) >> 5;
			
			instruction = group3opcode[aaa];
			addrmode = REL;
			
			operandinfo = getoperand(addrmode,instruction,opcode); //Produces operandinfo->operand and operandinfo->pagecross
			
			//printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
			printf("%s %04X (%s)",instructionnames[instruction],operandinfo.operand,addrmodenames[addrmode-100]);
			
			branch = exec(instruction,operandinfo.operand,addrmode,opcode);
			remainingcycles = getcyclecount(2, operandinfo.pagecross, branch, opcode); //2 is the base cycle for branch instructions
		break;
		case 4:
			aaa = (opcode & 0xF0) >> 4;
			
			instruction = group4opcode[aaa];
			addrmode = IMP;
			getoperand(addrmode,instruction,opcode);
			//printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
			printf("%s",instructionnames[instruction]);
			exec(instruction,0,addrmode,opcode);
			remainingcycles = group4cyclecount[aaa];
		break;
		case 5:
			aaa = (opcode & 0x70) >> 4;
			
			instruction = group5opcode[aaa];
			addrmode = IMP;
			getoperand(addrmode,instruction,opcode);
			//printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
			printf("%s",instructionnames[instruction]);
			exec(instruction,0,addrmode,opcode);
			remainingcycles = 2;
		break;
		case 6:
			switch(opcode)
				{
				case 0x20:
					operandinfo = getoperand(ABS,JSR,opcode); //Produces operandinfo->operand and operandinfo->pagecross
					remainingcycles = 6;
				break;
				case 0x00:
					instruction = BRK;
					remainingcycles = 7;
				break;
				case 0x40:
					instruction = RTI;
					remainingcycles = 6;
				break;
				case 0x60:
					instruction = RTS;
					remainingcycles = 6;
				break;
				}
			//printf("\ninstruction: %s\naddrmode: %s\noperand: %04X\npagecross: %d\n",instructionnames[instruction],addrmodenames[addrmode-100],operandinfo.operand,operandinfo.pagecross);
			printf("%s",instructionnames[instruction]);
			
			exec(instruction,operandinfo.operand,addrmode,opcode);
		break;
		}
	//printf("\n######Execution end#####\n");
	//printf("\nRequired cycles: %d\n",remainingcycles);
	//printf("\n######Decode end#####\n");
	//printf("\n######Results#####\n");
	printf("\nPC: %04X, SP: %02X, Accumulator: %02X, X: %02X, Y: %02X, P: %02X,",PC,SP,A,X,Y,P);
	printf("\nStack - 34: %02X, 33: %02X, 32: %02X, 31: %02X, 30: %02X",CPUMEM[0x0134],CPUMEM[0x0133],CPUMEM[0x0132],CPUMEM[0x0131],CPUMEM[0x0130]);
	//printf("\n######Results End#####\n");
	//printf("\n--------------------------------------------------------------------------\n\n");
	return remainingcycles;
	}
	
struct operandstruct getoperand(int addrmode, int instruction, int opcode)
	{
	int operand, pagecross= 0; //For relative mode
	uint8_t lowbyte = CPUMEM[PC+1];
	uint8_t highbyte = CPUMEM[PC+2];
	uint16_t tmpoperand =  ((highbyte << 8) | (lowbyte));
	
	//Other required variables
	uint8_t tmplowbyte, tmphighbyte;

	switch(opcode)
		{
		case 0x6C: //JMP indirect
			addrmode = INDIR;
		break;
		case  0x96: //STX zero page, Y
			addrmode = ZEROY;
		break;
		case 0xB6: //LDX zero page, Y
			addrmode= ZEROY;
		break;
		case 0xBE: //LDX absolute, Y
			addrmode = ABSY;
		break;
		}
		
	switch(addrmode)
		{
		case ZERO:
			operand = lowbyte;
			PC += 1;
		break;
		case ZEROX:
			lowbyte += X; //The uint8_t datatype does the wraparound for us
			operand = lowbyte; 
			PC += 1;
		break;
		case ZEROY:
			lowbyte += Y; //The uint8_t datatype does the wraparound for us
			operand = lowbyte;
			PC += 1;
		break;
		case ABS:
			operand = tmpoperand;
			PC += 2;
		break;
		case ABSX:
			operand = tmpoperand+X;
			if((tmpoperand & 0x1100) != ((tmpoperand+X) & 0x1100))
				{
				pagecross = 1;
				}
			PC += 2;
		break;
		case ABSY:
			operand = tmpoperand+Y;
			if((tmpoperand & 0x1100) != ((tmpoperand+Y) & 0x1100))
				{
				pagecross = 1;
				}
			PC += 2;
		break;
		case ACC:
			//Operates on the accumulator
		break;
		case IMM:
			operand = lowbyte;
			PC += 1;
		break;
		case REL: //We are going to modify the cycle count in exec() for branch instructions;
			operand = lowbyte;
			if(((PC+2) & 0x1100) != ((PC+2+lowbyte) & 0x1100)) //Is THIS how you calculate page cross?
				{
				pagecross = 1;
				}
			PC += 1;
		break;
		case INDIR:
			tmplowbyte = CPUMEM[tmpoperand];
			tmphighbyte = CPUMEM[tmpoperand+1];
			if((tmpoperand & 0x00FF) == 0x00FF)
				{
				tmphighbyte = CPUMEM[tmpoperand-0x00FF]; //Kind of a bug in the original 6502
				}
			operand = ((tmphighbyte << 8) | (tmplowbyte));
			PC += 2;
		break;
		case INDIRX:
			lowbyte += X; //Do wraparound
			tmplowbyte = CPUMEM[lowbyte];
			tmphighbyte = CPUMEM[lowbyte+1];
			operand = ((tmphighbyte << 8) | (tmplowbyte));
			PC += 1;
		break;
		case INDIRY:
			tmplowbyte = CPUMEM[lowbyte];
			tmphighbyte = CPUMEM[lowbyte+1];
			tmpoperand = ((tmphighbyte << 8) | (tmplowbyte));
			operand = tmpoperand+Y;
			if((tmpoperand & 0x1100) != ((tmpoperand+Y) & 0x1100)) //Is THIS how you calculate page cross?
				{
				pagecross = 1;
				}
			PC += 1;
		break;
		case IMP:
		break;
		default:
			printf("\nUnknown addrmode, original opcode: %02X\n",opcode);
		}
	
	PC++; //The instruction byte
	struct operandstruct result = {operand, pagecross};
	return result;
	}
	
	
int exec(int instruction, int operand, int addrmode, int opcode) //Immediate mode does not work with this method
	{
	//printf("\n#####Execution start#####\n");
		
	//Group by instruction type
	int intvar;
	uint8_t uint8var;
	uint8_t operandcontent;
	uint8_t lowbyte, highbyte;
	int branch = 0;
	
	//#######################################################################################################
	//YOU NEED TO SET THE CARRY, NEGATIVE, ZERO, AND OVERFLOW FLAGS TO 0 IF NOTHING HAPPENED TO THEM!!!!!!!!!
	//#######################################################################################################
	switch(opcode)
		{
		case 0x00: //BRK
			stackpush((PC & 0xFF00)/0x0100);
			stackpush(PC & 0x00FF);
			
			setflag(4,1);
			stackpush(P);
			setflag(4,0);
			PC = ((CPUMEM[0xFFFF] << 8) | (CPUMEM[0xFFFE]));
		break;
		case 0x20: //JSR
			stackpush((PC & 0xFF00)/0x0100);
			stackpush(PC & 0x00FF);
			
			PC = operand;
		break;
		case 0x40: //RTI
			P = stackpull();
			setflag(4,0);
			lowbyte = stackpull();
			highbyte = stackpull();
			
			PC = (lowbyte | (highbyte << 8));
		break;
		case 0x60: //RTS
			lowbyte = stackpull();
			highbyte = stackpull();
			
			PC = (lowbyte | (highbyte << 8));
		break;
			
		default:
			switch(instruction)
				{
				//Load/store
				case LDA:
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;	
						}
					A = operandcontent;
					setznflag(A);
				break;
				case LDX:
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
					X = operandcontent;
					setznflag(X);
				break;
				case LDY:
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;	
						}
					Y = operandcontent;
					setznflag(Y);
				break;
				case STA:
					CPUMEM[operand] = A;
				break;
				case STX:
					CPUMEM[operand] = X;
				break;
				case STY:
					CPUMEM[operand] = Y;
				break;
				
				//Register transfer
				case TAX:
					X = A;
					setznflag(X);
				break;
				case TAY:
					Y = A;
					setznflag(Y);
				break;
				case TXA:
					A = X;
					setznflag(A);
				break;
				case TYA:
					A = Y;
					setznflag(A);
				break;
				
				//Stack operations
				case TSX:
					X = SP;
					setznflag(X);
				break;
				case TXS:
					SP = X;
					//Does not affect any status bits
				break;
				case PHA:
					stackpush(A);
				break;
				case PHP:
					setflag(4,1); //Strange, but the internet says so
					stackpush(P);
					setflag(4,0);
				break;
				case PLA:
					A = stackpull();
					setznflag(A);
				break;
				case PLP:
					P = stackpull();
					setflag(4,0); //Restoring status flags ignore bit 4 and 5.
				break;
				
				//Logical
				case AND:
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;	
						}
					A &= operandcontent;
					setznflag(A);
				break;
				case EOR:
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;	
						}
					A ^= operandcontent;
					setznflag(A);
				break;
				case ORA:
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;	
						}
					A |= operandcontent;
					setznflag(A);
				break;
				case BIT:
					setflag(1,0);
					if((A&CPUMEM[operand]) == 0)
						{
						setflag(1,1);	
						}
					P &= 0x3F; //Clear bits 6 and 7
					P |= (CPUMEM[operand] & 0xC0); //Apply bits 6 and 7
				break;
				
				//Arithmetic
				case ADC:
					setflag(6,0); //This works here, bit for instructions that only affect zn, only reset zn.
					
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
					intvar = A + operandcontent + getflag(0); //Sum
					
					setflag(0,0); //THIS SHOULD NOT BE AT THE TOP, SINCE WE USE THE VALUE!
					
					if(intvar > 0xFF)
						{
						setflag(0,1);
						}
					uint8var = intvar; //Wraparound the sum
					if(((A & 0x80) == (operandcontent & 0x80)) && ((A & 0x80) != (uint8var & 0x80)))
						{
						setflag(6,1); //Set overflow
						}
					A = uint8var;
					setznflag(A);
				break;
				case SBC:
					/*
					setflag(6,0); //This works here, bit for instructions that only affect zn, only reset zn.
					
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
					intvar = A - operandcontent - (1-getflag(0)); //Sum
					
					setflag(0,0); //THIS SHOULD NOT BE AT THE TOP, SINCE WE USE THE VALUE!
					
					if(intvar < 0)
						{
						setflag(0,1);
						}
					uint8var = intvar; //Wraparound the sum
					if(((A & 0x80) != (operandcontent & 0x80)) && ((operandcontent & 0x80) == (uint8var & 0x80)))
						{
						setflag(6,1); //Set overflow
						}
					A = uint8var;
					setznflag(A);
					*/
					
					/* THIS IS FROM STACK OVERFLOW - https://stackoverflow.com/questions/29193303/6502-emulation-proper-way-to-implement-adc-and-sbc
					Here's the complete source for SBC in my emulator. All flags will be set correctly too.

						static void sbc(uint8_t arg) { adc(~arg);}
					*/
					
					setflag(6,0); //This works here, bit for instructions that only affect zn, only reset zn.
					
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
					
					operandcontent = ~operandcontent; //ONLY DIFFERENCE BETWEEN ADC AND SBC
					
					intvar = A + operandcontent + getflag(0); //Sum
					
					setflag(0,0); //THIS SHOULD NOT BE AT THE TOP, SINCE WE USE THE VALUE!
					
					if(intvar > 0xFF)
						{
						setflag(0,1);
						}
					uint8var = intvar; //Wraparound the sum
					if(((A & 0x80) == (operandcontent & 0x80)) && ((A & 0x80) != (uint8var & 0x80)))
						{
						setflag(6,1); //Set overflow
						}
					A = uint8var;
					setznflag(A);
				break;
				case CMP:
					setflag(0,0);
					
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
						
					uint8var = A-operandcontent;
					if(uint8var >= 0)
						{
						setflag(0,1);	
						}
					setznflag(uint8var);
				break;
				case CPX:
					setflag(0,0);
					
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
						
					uint8var = X-operandcontent;
					if(uint8var >= 0)
						{
						setflag(0,1);	
						}
					setznflag(uint8var);
				break;
				case CPY:
					setflag(0,0);
					
					operandcontent = CPUMEM[operand];
					if(addrmode == IMM)
						{
						operandcontent = operand;
						}
						
					uint8var = Y-operandcontent;
					if(uint8var >= 0)
						{
						setflag(0,1);	
						}
					setznflag(uint8var);
				break;
				
				//Increments/Decrements
				case INC:
					CPUMEM[operand] += 1;
					setznflag(CPUMEM[operand]);
				break;
				case INX:
					X += 1;
					setznflag(X);
				break;
				case INY:
					Y += 1;
					setznflag(Y);
				break;
				case DEC:
					CPUMEM[operand] -= 1;
					setznflag(CPUMEM[operand]);
				break;
				case DEX:
					X -= 1;
					setznflag(X);
				break;
				case DEY:
					Y -= 1;
					setznflag(Y);
				break;
				
				//Shifts
				case ASL:
					if(addrmode == ACC)
						{
						setflag(0,(A & 0x80)/0x80); //0 or 1
						A *= 2;
						setznflag(A);
						}
					else
						{
						setflag(0,(CPUMEM[operand] & 0x80)/0x80);
						CPUMEM[operand] *= 2;
						setznflag(CPUMEM[operand]);
						}
				break;
				case LSR:
					if(addrmode == ACC)
						{
						setflag(0,A & 0x1);
						A /= 2;
						setznflag(A);
						}
					else
						{
						setflag(0,CPUMEM[operand] & 0x1);
						CPUMEM[operand] /= 2;
						setznflag(CPUMEM[operand]);
						}
				break;
				case ROL:
					if(addrmode == ACC)
						{
						uint8var = A;
						A *= 2;
						A |= getflag(0);
						setflag(0,(uint8var & 0x80)/0x80);
						setznflag(A);
						}
					else
						{
						uint8var = CPUMEM[operand];
						CPUMEM[operand] *= 2;
						CPUMEM[operand] |= getflag(0);
						setflag(0,(uint8var & 0x80)/0x80);
						setznflag(CPUMEM[operand]);
						}
				break;
				case ROR:
					if(addrmode == ACC)
						{
						uint8var = A;
						A /= 2;
						A |= getflag(0)*0x80;
						setflag(0,(uint8var & 0x1));
						setznflag(A);
						}
					else
						{
						uint8var = CPUMEM[operand];
						CPUMEM[operand] /= 2;
						CPUMEM[operand] |= getflag(0)*0x80;
						setflag(0,(uint8var & 0x1));
						setznflag(CPUMEM[operand]);
						}
				break;
				
				//Jumps/Calls
				case JMP:
					PC = operand;
				break;
				
				//Branches
				case BCC:
					if(getflag(0) == 0)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BCS:
					if(getflag(0) == 1)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BEQ:
					if(getflag(1) == 1)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BMI:
					if(getflag(7) == 1)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BNE:
					if(getflag(1) == 0)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BPL:
					if(getflag(7) == 0)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BVC:
					if(getflag(6) == 0)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				case BVS:
					if(getflag(6) == 1)
						{
						if((operand & 0x80) == 0x80)
							{
							branch = 1;
							uint8var = 0x100-operand;
							PC -= uint8var;
							}
						else { PC += operand;}
						}
				break;
				
				//Status reguister changes
				case CLC:
					setflag(0,0);
				break;
				case CLD:
					setflag(3,0);
				break;
				case CLI:
					setflag(2,0);
				break;
				case CLV:
					setflag(6,0);
				break;
				case SEC:
					setflag(0,1);
				break;
				case SED:
					setflag(3,1);
				break;
				case SEI:
					setflag(2,1);
				break;
				
				//System instructions
				case NOP:
				break;
				}
		}
	setflag(5,1); //Bit 5 is always set
	//printf("\nBranch: %d\n",branch);
	return branch;
	}
	
int getcyclecount(int cycle, int pagecross, int branch, int opcode)
	{
	switch(opcode)
		{
		case 0x4C: //JMP ABS
			return 3;
		break;
		case 0x6C: //JMP INDIR
			return 5;
		break;
		case 0x86: //STX ZERO
			return 3;
		break;
		case 0x96: //STX ZEROY
			return 4;
		break;
		case 0x8E: //STX ABS
			return 4;
		break;
		case 0xA2: //LDX IMM
			return 2;
		break;
		case 0xA6: //LDX ZERO
			return 3;
		break;
		case 0xB6: //LDX ZEROY
			return 4;
		break;
		case 0xAE: //LDX ABS
			return 4;
		break;
		case 0xBE: //LDX ABSY
			return 4 + pagecross;
		break;
		default:
			return cycle + pagecross + branch;
		}
	}
