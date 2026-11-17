"""Executes only a tiny fake host-support fixture, never a synth or native module."""
import pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
SOURCE=r'''
#include "native_observer_support.hpp"
#include <cassert>
#include <cstdlib>
#include <new>
bool failAllocation=false;
void* operator new(std::size_t n) {if(failAllocation)throw std::bad_alloc{};if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
using namespace cs80_observer;
int main(int argc,char** argv) {
 require(argc==2,"case");std::string name=argv[1];
 if(name=="encoding") {Bytes b;b.u32(0x12345678);b.u64(0x0102030405060708ull);b.pcm(1.f);b.f64(-0.0);const std::vector<std::uint8_t> expected{0x78,0x56,0x34,0x12,8,7,6,5,4,3,2,1,0,0,0x80,0x3f,0,0,0,0,0,0,0,0x80};require(b.data==expected,"little endian bits");}
 else if(name=="utf16") {Bytes b;const char16_t s[]{0x0041,0x03b1,0};b.terminated(s);require(b.data.size()==12 && b.data[8]==65 && b.data[10]==0xb1 && b.data[11]==3,"utf16 fields");}
 else if(name=="unterminated") {Bytes b;const char s[]{'a','b'};bool rejected=false;try{b.terminated(s);}catch(...){rejected=true;}require(rejected,"unterminated");}
 else if(name=="nonfinite") {Bytes b;bool rejected=false;try{b.pcm(std::numeric_limits<float>::infinity());}catch(...){rejected=true;}require(rejected && b.data.empty(),"nonfinite");}
 else if(name=="state") {StateBuffer b;const char x[]{'a',0,'c'};b.write(x,3);b.seek(-2,2);b.write("Z",1);b.seek(0,0);char out[8]{};require(b.read(out,8)==3 && out[0]=='a' && out[1]=='Z' && out[2]=='c',"complete binary state");require(b.read(out,1)==0,"EOF");}
 else if(name=="bounds") {StateBuffer b;bool rejected=false;try{b.seek(-1,0);}catch(...){rejected=true;}require(rejected,"negative seek");rejected=false;try{b.write("x",StateBuffer::limit+1);}catch(...){rejected=true;}require(rejected && b.bytes.empty(),"oversized state");}
 else if(name=="null") {StateBuffer b;bool rejected=false;try{b.write(nullptr,1);}catch(...){rejected=true;}require(rejected,"null write");b.write(nullptr,0);}
 else if(name=="close") {std::vector<int> order;CheckedClose c;c.add("stop",[&]{order.push_back(1);return true;});c.add("release",[&]{order.push_back(2);return true;});require(!c.successful(),"not closed");c.close();require(c.successful() && order==std::vector<int>({1,2}),"ordered close");}
 else if(name=="close-failure") {std::vector<int> order;CheckedClose c;c.add("stop",[&]{order.push_back(1);return false;});c.add("release",[&]{order.push_back(2);return true;});bool rejected=false;try{c.close();}catch(...){rejected=true;}require(rejected && !c.successful() && order==std::vector<int>({1,2}) && !c.results[0].ok,"continue cleanup terminal failure");}
 else if(name=="close-throw") {CheckedClose c;bool released=false;c.add("stop",[]{throw std::runtime_error("failure");return true;});c.add("release",[&]{released=true;return true;});try{c.close();}catch(...){}require(released && !c.successful() && bool(c.results[0].error),"exception preserved");}
 else if(name=="close-retry") {CheckedClose c;int calls=0;c.add("close",[&]{++calls;return true;});c.close();bool rejected=false;try{c.close();}catch(...){rejected=true;}require(rejected && calls==1,"no repeated close");}
 else if(name=="close-destructor") {int calls=0;{CheckedClose c;c.add("close",[&]{++calls;return false;});}require(calls==1,"fallback cleanup");}
 else if(name=="close-allocation-failure") {CheckedClose c;bool released=false;c.add("allocate",[]{auto* p=::operator new(4);::operator delete(p);return true;});c.add("release",[&]{released=true;return true;});failAllocation=true;try{c.close();}catch(...){}failAllocation=false;require(released && !c.successful() && bool(c.results[0].error),"allocation failure must not abandon cleanup");}
 else if(name=="close-no-allocation") {CheckedClose c;bool released=false;c.add("release",[&]{released=true;return true;});failAllocation=true;c.close();failAllocation=false;require(released && c.successful(),"successful close performs no allocations");}
 else if(name=="duplicate") {CheckedClose c;c.add("close",[]{return true;});bool rejected=false;try{c.add("close",[]{return true;});}catch(...){rejected=true;}require(rejected,"duplicate operation");}
 else return 2;
}
'''
class NativeSupportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.root=pathlib.Path(cls.temp.name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Mandatory fake-host support checks require a C++ compiler')
        source=cls.root/'fixture.cpp';source.write_text(SOURCE);cls.exe=cls.root/'fixture'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-I',str(ROOT),str(source),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError('Fake-host support compilation failed: '+result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):subprocess.run([str(self.exe),name],check=True,capture_output=True,text=True,timeout=5)
for name in ('encoding','utf16','unterminated','nonfinite','state','bounds','null','close','close-failure','close-throw','close-retry','close-destructor','close-allocation-failure','close-no-allocation','duplicate'):
    setattr(NativeSupportTests,'test_'+name.replace('-','_'),lambda self,n=name:self.run_case(n))
if __name__=='__main__':unittest.main()
