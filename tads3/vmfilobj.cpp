/* 
 *   
 *   on using and copying this software.  
 *   statics 
 *   since we only need them to decode calls to call_stat_prop().  
 *   Create from stack 
    /* 
     *   static creator methods (openTextFile, openDataFile, openRawFile) 
 *   Create with no contents 
 *   Create with the given character set object and file handle.  
 *   Instantiate 
 *   Allocate and initialize our extension 
    /* 
     *   underlying system file, if any) 
    /* 
     *   if we also need a read buffer, figure in space for that as well. 
    /* 
     *   with and immediately following our extension, if we have one 
 *   Notify of deletion 
            /* 
 *   Get the data source 
 *   Get the file spec from the netfile 
/* 
 *   set a property 
/* 
 *   get a property 
/* 
 *   call a static property 
    /* 
     *   know which function we're calling 
 *   Static "open" methods 
 *   load from an image file 
    /* 
     *   access it (without storing it ourselves) during a reload 
 *   reload the object from image data 
 *   load or re-load image data 
    /* 
     *   environment 
    /* 
     *   of sync and thus cannot be read.  
/* 
 *   save to a file 
/* 
 *   restore from a file 
 *   Mark as referenced all of the objects to which we refer 
    /* 
     *   note that the stdio buffers have been invalidated 
        /* 
         *   buffers 
 *   seek before the write, and likewise if we write then read.  
    /* 
     *   we're performing the opposite type of operation now 
        /* 
         *   location to flush the stdio buffers 
    /* 
     *   we need to seek if we perform the opposite operation after this one 
 *   the local file system object. 
 *   
 *   
 *   when done with them.  
     *   mode from an extra integer argument.  
        /* write access; create or replace existing file */ 
        /* 
    
    /* 
     *   inherently constrained in the paths they can access.  
 *   Resolve a special file ID to a local filename path 
        
        
        
        
        
 *   create and return a mapper for the default character set. 
    
        
 *   Static property evaluator - open a text file 
    /* 
     *   storage server access if applicable 
        
                
                /* 
                 *   file to the section that contains the resource data 
                    /* 
                     *   seek within the resource later 
                    
                    /* 
                     *   the extent from the resource map 
                    
                /* 
                 *   fixed up to local conventions.  
            
            
            
            
            
            
            
            
            
        
        /* 
         *   we no longer need to delete it 
        /* 
         *   to the File object, we're abandoning the object.  
 *   Generic binary file opener - common to 'data' and 'raw' files 
                
                /* 
                 *   file to the section that contains the resource data 
                    /* 
                     *   resource stream later on 
                    
                    /* 
                     *   within a larger physical file 
                    
                
            
            
            
            
            
            
            
            
            
            
            
        
        /* 
         *   so we no longer have responsibility for it 
        /* 
         *   to the new File object, we're abandoning it 
 *   If the access is not allowed, we'll throw an error.  
    /* 
     *   read/write operations are allowed, as is delete. 
     *   the storage server.  Local file safety settings don't apply. 
    
    /*   
        /* 
 *   Check file safety for opening the given local file path. 
 *   if it's prohibited. 
    /* 
        /* 
        /* 
    /* 
    /* 
     *   sandbox folder. 
     *   
     *   
        /* 
         *   of the sandbox for this mode 
    /* 
 *   Property evaluator - get the character set 
 *   Property evaluator - set the character set 
 *   Check that we're in a valid state to perform file operations. 
 *   Check that we have read access 
 *   Check that we have write access 
 *   Check that we're in raw mode 
 *   Check that we've a valid raw readable file 
 *   Check that we've a valid raw writable file 
    /* 
     *   don't need to delete it again when the File object is collected 
    /* 
     *   to check with an explicit flush if we want this information. 
        /* 
        /* 
         *   it exists) with corrupt data. 
 *   Read a value in text mode 
    /* 
     *   buffer up or down as needed. 
     *   chunks, since the line might end up being longer than our buffer.  
        
            
    /* 
     *   for garbage collection protection 
 *   Read a value in 'data' mode 
        /* 
         *   includes the length prefix bytes 
        /* 
         *   store, not the length prefix part) 
    /* 
     *   stream, so throw an I/O error 
 *   Internal read routine 
    /* 
     *   for now to protect against losing it in garbage collection 
    
    
 *   Write a value in 'data' mode 
        
        
        /* 
         *   the bytes of the prefix itself in the length count 
         *   other types, try converting to a string. 
            
            
            /* 
             *   writer, which will never invoke garbage collection.  
        /* 
         *   file.  
        
        
 *   Property evaluator - read raw bytes from the file 
    
    
    
    /* 
     *   actually manage to read 
 *   Property evaluator - write raw bytes to the file 
        /* 
         *   return true 
        /* 
         *   we just leave well enough alone in that case 
 *   Get the current file position 
    /* 
     *   bytes remaining in the read buffer 
 *   Set position - internal version 
 *   Property evaluator - get size 
 *   Get the size, internal version 
 *   Get the mode, internal version 
    
    
        /* 
         *   to do the character set translation 
        /* 
         *   mode, so if the argument is present, simply discard it 
    
    /* 
     *   extension, so that they're not deleted when we reallocate it 
    /* 
     *   for the new file mode. 
 *   string.  This is a static (class) method.  
     *   the local OS API to get the root name.  
        /* 
         *   the part after the '/', otherwise it's the whole string.  
/* 
 *   property evaluator - unpackBytes 
        
/* 
    
/* 
/* 
 *   on return, false if we're at end of file.  
    
    /* 
     *   buffer 
 *   Read a character 
        /* 
         *   our buffer; if that fails, we're at EOF 
        /* 
         *   our buffer; if that fails, we're at EOF 